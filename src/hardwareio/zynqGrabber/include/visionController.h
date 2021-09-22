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

#ifndef __VVISCTRL__
#define __VVISCTRL__

#include <deviceRegisters.h>
#include <yarp/os/Bottle.h>

#include <string>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>

typedef struct fpgaStatus {
    bool crcErr;
    bool biasDone;
    bool i2cTimeout;
    bool apsFifoFull;
    bool tdFifoFull;
} fpgaStatus_t;

class vVisionCtrl
{
private:

    //PARAMETERS
    std::string deviceName;
    std::string deviceType;
    unsigned char I2CAddress;

    //INTERNAL VARIABLES
    int fd;
    yarp::os::Bottle bias;
    fpgaStatus_t fpgaStat;
    bool iBias;
    bool aps;

    //INTERNAL FUNCTIONS
    int i2cRead(unsigned char reg, unsigned char *data, unsigned int size);
    int i2cWrite(unsigned char reg, unsigned char *data, unsigned int size);
    int ReadSisleyRegister(uint32_t sisley_reg_address, uint32_t *sisley_data);
    int WriteSisleyRegister(uint32_t sisley_reg_address, uint32_t sisley_data);
    bool EnablePower();
    bool SisleySetup();
    int SetROI(int start, int size, xory_t coord, tdorem_t type);

    bool configureBiaseseGen3();
    bool configureBiasesGen1();

    bool configureRegistersGen1();
    bool configureRegistersGen3();

    bool SetupVSCTRLinHSSAERmode();
    bool enableGTP();

    //WRAPPERS?
    bool configureRegisters(); //new initDevice

    bool setLatchAtEnd(bool Enable);
    bool setShiftCount(uint8_t shiftCount);

    int getFpgaStatus();
    bool clearFpgaStatus(std::string clr);

    bool checkBiasProg();

public:

    //REQUIRE: devicefilename, chiptype (eg DVS/ATIS), chipFPGAaddress (eg LEFT or RIGHT)
    vVisionCtrl(std::string deviceName = "", std::string deviceType = "", unsigned char i2cAddress = 0);

    //SET/GET CONFIGURATION
    bool setBias(std::string biasName, unsigned int biasValue);
    bool setBias(yarp::os::Bottle bias);
    unsigned int getBias(std::string biasName);
    void useCurrentBias(bool flag = true);
    void turnOnAPS(bool flag = true);
    bool activateAPSShutter();

    //CONNECTION
    bool connect(void);
    bool configure(bool verbose = false);
    void disconnect(bool andturnoff = false);
    bool activate(bool active = true);
    bool suspend(void); //wraps activate(false);

    //COMMANDS
    bool configureBiases();
    bool SisleyTDROIDefinition(int x, int y, int width, int height);

    //DEBUG OUTPUTS
    void printConfiguration(void); // bias file, void dumpRegisterValues();


};

#endif
