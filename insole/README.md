# Insole Board

Relevant Pins

| SAMR Pin | Purpose | Initialize |
|----------|---------|------------|
| `PA06`   | ADC Witricity rectified voltage | |
| `PA07`   | ADC Fan voltage | |
| `PA08`   | ADC Battery voltage | |
| `PA18`   | I2C port expander RST | Active LOW |
| `PA19`   | Low Batt Indicator from Boost | LOW when battery is low |
| `PA25`   | Button input for blower/warmer (wrist) | |
| `PA27`   | ENABLE for Boost | Active HIGH |

## TODO:
- [ ] check if the AONs work


### I2C Port Expander

`RST` has pull-down resistor which should keep the switches OPEN. When the board starts up, it should pull `PA18` high

### Battery

Need to do one of the following:
- if external power *not* connected: short VSS to V- when the battery is connected 
- if external power *is* connected: can just plug in the battery, and then remove external power

## System

The insole board will report data as well as receive actuation commands

### Data Reporting

We can monitor the following

- Witricity rectified voltage (4-33 volts or whatever):
    - scalar (waiting on SAMR21 ADC)
- Battery voltage:
    - scalar (waiting on SAMR21 ADC)
- the low-battery indicator:
    - binary 
- connected to witricity:
    - binary (waiting on SAMR21 ADC)
- on/off state for each insole element (6):
    - 3-tuple (binary state, scalar frequency, scalar duty cycle)
- accelerometer value:
    - 3-tuple (x,y,z)
- temperature value:
    - scalar

### Actuation

Have an I2C port expander [PCA9557](http://www.nxp.com/documents/data_sheet/PCA9557.pdf)

Address: `0011000_` (`_` is R/W) is (`0x18 << 1`)

#### Insole

- 6 actuation points
- only 2 on at any time, but arbitrary pairs
- for each pair, want to specify:
    - period, duty cycle: ('1s','40%') => 4s on, 6s off


## Implementation

- [ ] data reporting:
    - [ ] witricity voltage ADC (waiting on SAMR21 ADC)
    - [ ] battery voltage ADC (waiting on SAMR21 ADC)
    - [X] Low batt indicator
    - [ ] witiricty connectivity indicator (waiting on SAMR21 ADC)
    - [ ] insole element state
    - [X] accelerometer value
    - [X] temperature value
