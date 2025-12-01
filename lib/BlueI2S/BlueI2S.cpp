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
#include <Arduino.h>
#include "BlueI2S.h"
#include "blue_pio_i2s.pio.h"
#include <pico/stdlib.h>


I2S::I2S() {
    _running = false;
    _div_int = 48;
    _div_frac = 0;
    _bps = 16;
#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)
    _pio = pio2_hw;
    _sm = 1;
    _pinBCLK = 37;
    _pinDOUT = 39;
#else
    _pio = pio0_hw;
    _sm = 1;
    _pinBCLK = 26;
    _pinDOUT = 28;
#endif
}

I2S::~I2S() {
    end();
}

bool I2S::setBCLK(pin_size_t pin) {
#if defined(BLUESCSI_ULTRA) || defined (BLUESCSI_ULTRA_WIDE)
    if (_running) {
#else
    if (_running || (pin > 28)) {
#endif
        return false;
    }
    _pinBCLK = pin;
    return true;
}

bool I2S::setDATA(pin_size_t pin) {
#if defined(BLUESCSI_ULTRA) || defined (BLUESCSI_ULTRA_WIDE)
    if (_running) {
#else
    if (_running || (pin > 29)) {
#endif
        return false;
    }
    _pinDOUT = pin;
    return true;
}

bool I2S::setBitsPerSample(int bps) {
    if (_running || ((bps != 8) && (bps != 16) && (bps != 24) && (bps != 32))) {
        return false;
    }
    _bps = bps;
    return true;
}

volatile void *I2S::getPioFIFOAddr()
{
    return (volatile void *)&_pio->txf[_sm];
}

bool I2S::setDivider(uint16_t div_int, uint8_t div_frac) {
    _div_int = div_int;
    _div_frac = div_frac;
    return true;
}

uint I2S::getPioDreq() {
    return pio_get_dreq(_pio, _sm, true);
}

bool I2S::begin(PIO pio, uint sm) {
    if (_running)
        return true;
    _pio = pio;
    _sm = sm;
    _running = true;
    int off = 0;
    pio_sm_claim(_pio, _sm);
#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)
    pio->gpiobase = 16;
#endif
    off = pio_add_program(_pio, &pio_i2s_out_program);
    pio_i2s_out_program_init(_pio, _sm, off, _pinDOUT, _pinBCLK, _bps);
    pio_sm_set_clkdiv_int_frac(_pio, _sm, _div_int, _div_frac);
    pio_sm_set_enabled(_pio, _sm, true);
    return true;
}

void I2S::end() {
    if (_running) {
        pio_sm_set_enabled(_pio, _sm, false);
        _running = false;
    }
}
