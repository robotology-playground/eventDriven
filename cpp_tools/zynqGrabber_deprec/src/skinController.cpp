/*
 *   Copyright (C) 2017 Event-driven Perception for Robotics
 *   Author: arren.glover@iit.it
 *           chiara.bartolozzi@iit.it
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "skinController.h"
#include "deviceRegisters.h"
#include <yarp/os/Time.h>
#include <iostream>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <iostream>
#include <yarp/os/LogStream.h>

using yarp::os::Value;

vSkinCtrl::vSkinCtrl(std::string deviceName, unsigned char i2cAddress)
{

    fd = -1;
    this->deviceName = deviceName;
    this->I2CAddress = i2cAddress;

}

bool vSkinCtrl::connect()
{

    std::cout << "Connecting to " << deviceName << " for " << (int)I2CAddress << " device configuration" << std::endl;
    fd = open(deviceName.c_str(), O_RDWR);
    if (fd < 0) {
        perror("Cannot open device: ");
        return false;
    }

    return true;
}

//void vSkinCtrl::disconnect(bool andturnoff)
void vSkinCtrl::disconnect()
{
    if(fd > 0) {
        close(fd);
    }
}

int vSkinCtrl::i2cWrite(unsigned char reg, unsigned int data)
{
    int ret = ioctl(fd, I2C_SLAVE, I2CAddress);
    unsigned char tmp[5];

    tmp[0]= reg|AUTOINCR;
    tmp[1]= data & 0xFF;
    tmp[2]= data>>8 & 0xFF;
    tmp[3]= data>>16 & 0xFF;
    tmp[4]= data>>24 & 0xFF;

    ret = write (fd, tmp, 5);
    return (ret-1); // -1 because of one is the starting register

}

int vSkinCtrl::i2cWrite(unsigned char reg, unsigned char *data, unsigned int size)
{

    int ret = ioctl(fd, I2C_SLAVE, I2CAddress);

    unsigned char *tmp = (unsigned char *) malloc ((size+1)*sizeof(unsigned char));

    tmp[0]= size>1 ? reg|AUTOINCR : reg;

    for (unsigned int i = 0; i < size; i++)
        tmp[1+i]=data[i];

    ret = write (fd, tmp, size+1);

    free(tmp);

    return (ret-1); // -1 because of one is the starting register

}

int vSkinCtrl::i2cRead(unsigned char reg, unsigned char *data, unsigned int size)
{

    int ret = ioctl(fd, I2C_SLAVE, I2CAddress);
    unsigned char addr =  size>1 ? reg|AUTOINCR : reg;

    ret = write(fd, &addr, 1);
    if (ret<0)
        return ret;

    ret = read(fd, data, size);

    return ret;

}

bool vSkinCtrl::configure()
{
    if(!setDefaultRegisterValues())
        return false;
    std::cout << deviceName << ":" << (int)I2CAddress << " registers configured." << std::endl;
    printConfiguration();
    printFpgaStatus();

    return true;
}

bool vSkinCtrl::select_generator(int type, int neural_mask)
{
    unsigned char reg_val;
    if(i2cRead(SKCTRL_GEN_SELECT, &reg_val, 1) < 0)
        return false;

    reg_val = (reg_val & 0xE0) | type | (neural_mask << 2);

    if(i2cWrite(SKCTRL_GEN_SELECT, &reg_val, 1) < 0)
       return false;

    return true;
}

bool vSkinCtrl::config_generator(int type, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4)
{

    unsigned char reg_val;
    if(i2cRead(SKCTRL_GEN_SELECT, &reg_val, 1) < 0)
        return false;

    reg_val = (reg_val & 0x1F) | (type << 5);
    if(i2cWrite(SKCTRL_GEN_SELECT, &reg_val, 1) < 0)
        return false;

    if(i2cWrite(SKCTRL_EG_PARAM1_ADDR, p1) < 0)
        return false;

    if(i2cWrite(SKCTRL_EG_PARAM2_ADDR, p2) < 0)
        return false;

    if(i2cWrite(SKCTRL_EG_PARAM3_ADDR, p3) < 0)
        return false;

    if(i2cWrite(SKCTRL_EG_PARAM4_ADDR, p4) < 0)
        return false;

    return true;


}

bool vSkinCtrl::configureRegisters(yarp::os::Bottle cnfgReg)
{
    yInfo() << cnfgReg.toString();
    //SKIN CONTROL ENABLE REGISTER
    unsigned char regAddr = SKCTRL_EN_ADDR;

    std::string regName = "asrFilterType";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asBool();
        if(!setRegister(3,ASR_FILTER_TYPE,regAddr,regVal))
            return false;
    }
    regName = "asrFilterEn";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asBool();
        if(!setRegister(3,ASR_FILTER_EN,regAddr,regVal))
            return false;
    }
    regName = "egNthrEn";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asBool();
        if(!setRegister(3,EVGEN_NTHR_EN,regAddr,regVal))
            return false;
    }
    regName = "preprocSamples";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asBool();
        if(!setRegister(3,PREPROC_SAMPLES,regAddr,regVal))
            return false;
    }
    regName = "preprocEg";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asBool();
        if(!setRegister(3,PREPROC_EVGEN,regAddr,regVal))
            return false;
    }
    regName = "driftCompEn";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asBool();
        if(!setRegister(3,DRIFT_COMP_EN,regAddr,regVal))
            return false;
    }
    regName = "samplesSel";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asBool();
        if(!setRegister(3,SAMPLES_SEL,regAddr,regVal))
            return false;
    }
    regName = "samplesTxEn";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asBool();
        if(!setRegister(3,SAMPLES_TX_EN,regAddr,regVal))
            return false;
    }
    regName = "eventsTxEn";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asBool();
        if(!setRegister(3,EVENTS_TX_EN,regAddr,regVal))
            return false;
    }
    regName = "eventsTxEn";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asBool();
        if(!setRegister(3,EVENTS_TX_EN,regAddr,regVal))
            return false;
    }
    regName = "samplesTxMode";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asBool();
        if(!setRegister(3,SAMPLES_TX_MODE,regAddr,regVal))
            return false;
        if (regVal == false){
            regName = "samplesRshift";
            if (cnfgReg.check(regName)){
                bool regVal = cnfgReg.find(regName).asInt32();
                if(!setRegister(3,SAMPLES_RSHIFT,regAddr,regVal))
                    return false;
            }
        }
    }

    regAddr = SKCTRL_RES_TO_ADDR;
    regName = "resamplingTimeout";
    if (cnfgReg.check(regName)){
        bool regVal = cnfgReg.find(regName).asFloat64();
        if(i2cWrite(regAddr, (unsigned char *)(&regVal), sizeof(int)) < 0)
           return false;
    }

    //EVENT GENERATION SELECT
    regName = "evGenSel";
    if(cnfgReg.check(regName)) {

        int type = cnfgReg.find(regName).asInt32();
        int mask = 0;
        uint32_t p1, p2, p3, p4;

        switch(type) {

        case EV_GEN_1:

            p1 = FIXED_UINT(cnfgReg.check("G1upthresh", Value(0.1)).asFloat64());
            p2 = FIXED_UINT(cnfgReg.check("G1downthresh", Value(0.1)).asFloat64());
            p3 = FIXED_UINT(cnfgReg.check("G1upnoise", Value(12.0)).asFloat64());
            p4 = FIXED_UINT(cnfgReg.check("G1downnoise", Value(12.0)).asFloat64());
            yInfo() << "Setting Event Generator v1" << p1 << p2 << p3 << p4;
            config_generator(EV_GEN_1, p1, p2, p3, p4);
            break;

        case EV_GEN_2:

            p1 = FIXED_UINT(cnfgReg.check("G2upthresh", Value(50.0)).asFloat64());
            p2 = FIXED_UINT(cnfgReg.check("G2downthresh", Value(50.0)).asFloat64());
            p3 = FIXED_UINT(cnfgReg.check("G2upnoise", Value(50.0)).asFloat64());
            p4 = FIXED_UINT(cnfgReg.check("G2downnoise", Value(50.0)).asFloat64());
            yInfo() << "Setting Event Generator v2" << p1 << p2 << p3 << p4;
            config_generator(EV_GEN_2, p1, p2, p3, p4);
            break;

        case EV_GEN_NEURAL:

            if(cnfgReg.check("evNeuralUseSA1")) {

                p1 = cnfgReg.check("SA1inhibit", Value(524288)).asInt32();
                p2 = cnfgReg.check("SA1adapt", Value(328)).asInt32();
                p3 = cnfgReg.check("SA1decay", Value(-328)).asInt32();
                p4 = cnfgReg.check("SA1rest", Value(2621)).asInt32();
                config_generator(EV_GEN_SA1, UNSIGN_BITS(p1), UNSIGN_BITS(p2),
                                UNSIGN_BITS(p3), UNSIGN_BITS(p4));
                yInfo() << "Setting Event Generator SA1" << p1 << p2 << p3 << p4;
                mask = EV_MASK_SA1;

            } else if(cnfgReg.check("evNeuralUseRA1")) {

                p1 = cnfgReg.check("RA1inhibit", Value(327680)).asInt32();
                p2 = cnfgReg.check("RA1adapt", Value(3)).asInt32();
                p3 = cnfgReg.check("RA1decay", Value(-6552)).asInt32();
                p4 = cnfgReg.check("RA1rest", Value(65536)).asInt32();
                config_generator(EV_GEN_RA1, UNSIGN_BITS(p1), UNSIGN_BITS(p2),
                                 UNSIGN_BITS(p3), UNSIGN_BITS(p4));
                yInfo() << "Setting Event Generator RA1" << p1 << p2 << p3 << p4;
                mask = EV_MASK_RA1;

            } else if(cnfgReg.check("evNeuralUseRA2")) {

                 p1 = cnfgReg.check("RA2inhibit", Value(327680)).asInt32();
                 p2 = cnfgReg.check("RA2adapt", Value(3)).asInt32();
                 p3 = cnfgReg.check("RA2decay", Value(-3276)).asInt32();
                 p4 = cnfgReg.check("RA2rest", Value(2621)).asInt32();
                 config_generator(EV_GEN_RA2, UNSIGN_BITS(p1), UNSIGN_BITS(p2),
                                  UNSIGN_BITS(p3), UNSIGN_BITS(p4));
                 yInfo() << "Setting Event Generator RA2" << p1 << p2 << p3 << p4;
                 mask = EV_MASK_RA2;
            } else {
                yWarning() << "Neural Generator Selected with specifying which "
                              "generator to use";
            }
            break;

        default:
            yWarning() << "Error in specifying event generator type";

        }

        if(!select_generator(type, mask))
            return false;
    }

    printConfiguration();

    return true;
}

bool vSkinCtrl::setRegister(int byte, unsigned int mask, unsigned char regAddr, bool regVal)
{
    unsigned int val;
    if(i2cRead(regAddr, (unsigned char*)&val, sizeof(val))<0)
        return false;

    if (regVal == true) {
        val |= (mask << 8*byte);
    } else {
        val &= ~(mask << 8*byte);
    }

    if(i2cWrite(regAddr, (unsigned char *)(&val), sizeof(int)) < 0)
        return false;

    return true;
}
bool vSkinCtrl::setRegister(unsigned char regAddr, double regVal)
{

    if(i2cWrite(regAddr, (unsigned char *)(&regVal), sizeof(int)) < 0)
        return false;

    return true;
}

bool vSkinCtrl::calibrate()
{
    yInfo() << "Performing Skin Calibration ... (don't touch!)";
    unsigned char valReg;
    if(i2cRead(SKCTRL_EN_ADDR, &valReg, 1) < 0)
        return false;

    unsigned char reg_with_calib = valReg;
    reg_with_calib |= FORCE_CALIB_EN;
    if(i2cWrite(SKCTRL_EN_ADDR, &reg_with_calib, 1) < 0)
        return false;

    yarp::os::Time::delay(1.0);

    if(i2cWrite(SKCTRL_EN_ADDR, &valReg, 1) < 0)
        return false;

    yInfo() << "Calibration done";

    return true;

}

bool vSkinCtrl::setDefaultRegisterValues()
{

    if(!calibrate())
        return false;

    unsigned char valReg[4];

    // --- configure SKCTRL_EN_ADDR --- //
    valReg[0] = I2C_ACQ_EN;
    // I2C acquisition enable | force calibration | Dummy generators general enable | Current dummy generator select
    valReg[1] = 0;  // Disable dummy generator (e.g. which dummy generator you want to enable, from 0 to 7)
    valReg[2] = EVGEN_NTHR_EN|PREPROC_SAMPLES|PREPROC_EVGEN;
    // Event Generator configuration
    // Enable and configure events and Samples
    unsigned char rshift = (SAMPLES_RSHIFT_DEFAULT << SAMPLES_RSHIFT_SHIFT)&SAMPLES_RSHIFT;
    valReg[3] = rshift|SAMPLES_SEL;
    if(i2cWrite(SKCTRL_EN_ADDR, valReg, 4) < 0) return false;

    // --- configure SKCTRL_EN_ADDR --- //
    valReg[0] = EV_GEN_SELECT_DEFAULT;
    if(i2cWrite(SKCTRL_GEN_SELECT, valReg, 1) < 0) return false;

    // --- configure SKCTRL_DUMMY_PERIOD_ADDR --- //
    if(i2cWrite(SKCTRL_DUMMY_PERIOD_ADDR, DUMMY_PERIOD_DEFAULT) < 0) return false;

    // --- configure SKCTRL_DUMMY_CFG_ADDR --- //
    valReg[0] = LOW8(DUMMY_CALIB_DEFAULT);                   //
    valReg[1] = HIGH8(DUMMY_CALIB_DEFAULT);                    //
    valReg[2] = LOW8(DUMMY_ADDR_DEFAULT);                    //
    valReg[3] = HIGH8(DUMMY_ADDR_DEFAULT);                     //
    if(i2cWrite(SKCTRL_DUMMY_CFG_ADDR, valReg, 4) < 0) return false;

    // --- configure SKCTRL_DUMMY_BOUND_ADDR --- //
    valReg[0] = LOW8(DUMMY_UP_BOUND_DEFAULT);                //
    valReg[1] = HIGH8(DUMMY_UP_BOUND_DEFAULT);                 //
    valReg[2] = LOW8(DUMMY_LOW_BOUND_DEFAULT);               //
    valReg[3] = HIGH8(DUMMY_LOW_BOUND_DEFAULT);                //
    if(i2cWrite(SKCTRL_DUMMY_BOUND_ADDR, valReg, 4) < 0) return false;


    // --- configure SKCTRL_DUMMY_INC_ADDR --- //
    valReg[0] = LOW8(DUMMY_INC_DEFAULT);  //
    valReg[1] = HIGH8(DUMMY_INC_DEFAULT);  //
    valReg[2] = LOW8(DUMMY_DECR_DEFAULT);  //
    valReg[3] = HIGH8(DUMMY_DECR_DEFAULT);  //
    if(i2cWrite(SKCTRL_DUMMY_INC_ADDR, valReg, 4) < 0) return false;

    // --- configure SKCTRL_RES_TO_ADDR --- //
    if(i2cWrite(SKCTRL_RES_TO_ADDR, RESAMPLING_TIMEOUT_DEFAULT) < 0) return false;


    config_generator(EV_GEN_2,
                       FIXED_UINT(EG_UP_THR_DEFAULT),
                       FIXED_UINT(EG_DWN_THR_DEFAULT),
                       FIXED_UINT(EG_NOISE_RISE_THR_DEFAULT),
                       FIXED_UINT(EG_NOISE_FALL_THR_DEFAULT));

    // --- configure SKCTRL_I2C_ACQ_SOFT_RST_ADDR --- //
    if(i2cWrite(SKCTRL_I2C_ACQ_SOFT_RST_ADDR, I2C_ACQ_SOFT_RST_DEFAULT) < 0) return false;

    yInfo() << "Finished Default Register Configuration";
    return true;
}

int vSkinCtrl::printFpgaStatus()
{

    unsigned int val;
    int ret = i2cRead(SKCTRL_STATUS_ADDR, (unsigned char*)&val, sizeof(val));

    yInfo() << "ED-MTB skin type: " << (val & SKCTRL_EDMTB_SKIN_TYPE_MSK);
    yInfo() << "TX keep alive: " << (val & SKCTRL_TX_KEEPALIVE_EN_MSK);
    yInfo() << "I2C cfg table length: " << (val & SKCTRL_I2C_CFG_TABLE_LEN_MSK);
    yInfo() << "I2C cfg filter taps: " << (val & SKCTRL_I2C_CFG_FILTER_TAPS_MSK);
    yInfo() << "I2C cfg SCL freq: " << (val & SKCTRL_I2C_CFG_SCL_FREQ_MSK);
    yInfo() << "I2C cfg SDA number: " << (val & SKCTRL_I2C_CFG_SDA_N_MSK);
    yInfo() << "FPGA minor: " << (val & SKCTRL_MINOR_MSK);
    yInfo() << "FPGA major: " << (val & SKCTRL_MAJOR_MSK);

    return ret;
}


void vSkinCtrl::printConfiguration()
{

    std::cout << std::endl << "== FPGA Register Values ==" << std::endl;
    unsigned int regval = 0;
    unsigned char small_reg_val = 0;

    i2cRead(SKCTRL_VERSION_MAJ, &small_reg_val, sizeof(small_reg_val));
    printf("Version: %d.", small_reg_val);
    i2cRead(SKCTRL_VERSION_MIN, &small_reg_val, sizeof(small_reg_val));
    printf("%d\n", small_reg_val);

    i2cRead(SKCTRL_EN_ADDR, (unsigned char *)&regval, sizeof(regval));
    printf("Enable Register: 0x%08X\n", regval);
    i2cRead(SKCTRL_GEN_SELECT, &small_reg_val, sizeof(small_reg_val));
    printf("Generator Select Register: 0x%02X\n", small_reg_val);
    i2cRead(SKCTRL_DUMMY_PERIOD_ADDR, (unsigned char *)&regval, sizeof(regval));
    printf("Dummy Generator Period: 0x%08X\n", regval);
    i2cRead(SKCTRL_DUMMY_CFG_ADDR, (unsigned char *)&regval, sizeof(regval));
    printf("Dummy Generator Calib and Address: 0x%08X\n", regval);
    i2cRead(SKCTRL_DUMMY_BOUND_ADDR, (unsigned char *)&regval, sizeof(regval));
    printf("Dummy Generator Upper and Lower Bounds: 0x%08X\n", regval);
    i2cRead(SKCTRL_DUMMY_INC_ADDR, (unsigned char *)&regval, sizeof(regval));
    printf("Dummy Generator Increment and Decrement: 0x%08X\n", regval);
    i2cRead(SKCTRL_RES_TO_ADDR, (unsigned char *)&regval, sizeof(regval));
    printf("Resampling Time Out: 0x%08X\n", regval);
    i2cRead(SKCTRL_EG_PARAM1_ADDR, (unsigned char *)&regval, sizeof(regval));
    printf("Event generator P1: 0x%08X\n", regval);
    i2cRead(SKCTRL_EG_PARAM2_ADDR, (unsigned char *)&regval, sizeof(regval));
    printf("Event generator P2: 0x%08X\n", regval);
    i2cRead(SKCTRL_EG_PARAM3_ADDR, (unsigned char *)&regval, sizeof(regval));
    printf("Event generator P3: 0x%08X\n", regval);
    i2cRead(SKCTRL_EG_PARAM4_ADDR, (unsigned char *)&regval, sizeof(regval));
    printf("Event generator p4: 0x%08X\n", regval);
    i2cRead(SKCTRL_EG_FILTER_ADDR, (unsigned char *)&regval, sizeof(regval));
    printf("Resampling/evgen filter address: 0x%08X\n", regval);

    std::cout << std::endl;

}




