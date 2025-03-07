/*
    I2SIn and I2SOut for Raspberry Pi Pico
    Implements one or more I2S interfaces using DMA

    Copyright (c) 2022 Earle F. Philhower, III <earlephilhower@yahoo.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

class I2S {
public:
    I2S();
    virtual ~I2S();

    bool setBCLK(pin_size_t pin);
    bool setDATA(pin_size_t pin);
    bool setBitsPerSample(int bps);

    bool setDivider(uint16_t div_int, uint8_t div_frac);
    uint getPioDreq();
    volatile void *getPioFIFOAddr();

    bool begin(PIO pio, uint sm);
    void end();

private:
    pin_size_t _pinBCLK;
    pin_size_t _pinDOUT;
    uint16_t _div_int;
    uint8_t _div_frac;
    int _bps;
    bool _running;

    PIOProgram *_i2s;
    PIO _pio;
    int _sm;
};
