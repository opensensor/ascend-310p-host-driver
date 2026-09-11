/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* Description:
* Author: huawei
* Create: 2019-10-15
*/

#include "dms_sensor_type.h"


#define DMS_DECLARE_SEN_TYPE_START(type) static struct dms_sensor_type_string _g_event_type_##type[] = {
#define DMS_DECLARE_SEN_TYPE_END }

#define DMS_ADD_SENSOR_TYPE(type, name)                                                                        \
    {                                                                                                          \
        type, name, sizeof(_g_event_type_##type) / sizeof(struct dms_sensor_type_string), _g_event_type_##type \
    }

/* Temperature sensor tryp:01h,offset:00H-0BH,all:12,(de)asserted */
DMS_DECLARE_SEN_TYPE_START(0x01)
    {0x00, 0x01, "Crossed Lower non-critical going low. "},
    {0x01, 0x01, "Crossed Lower non-critical going high. "},
    {0x02, 0x02, "Crossed Lower critical going low. "},
    {0x03, 0x02, "Crossed Lower critical going high. "},
    {0x04, 0x03, "Crossed Lower non-recoverable going low. "},
    {0x05, 0x03, "Crossed Lower non-recoverable going high. "},
    {0x06, 0x01, "Crossed Upper non-critical going low. "},
    {0x07, 0x01, "Crossed Upper non-critical going high. "},
    {0x08, 0x02, "Crossed Upper critical going low. "},
    {0x09, 0x02, "Crossed Upper critical going high. "},
    {0x0A, 0x03, "Crossed Upper non-recoverable going low. "},
    {0x0B, 0x03, "Crossed Upper non-recoverable going high. "},
DMS_DECLARE_SEN_TYPE_END;

    /* VOLTAGE Sensor type */
    DMS_DECLARE_SEN_TYPE_START(0x02)
        {0x00, 0x01, "Crossed Lower non-critical going low. "},
        {0x01, 0x01, "Crossed Lower non-critical going high. "},
        {0x02, 0x02, "Crossed Lower critical going low. "},
        {0x03, 0x02, "Crossed Lower critical going high. "},
        {0x04, 0x03, "Crossed Lower non-recoverable going low. "},
        {0x05, 0x03, "Crossed Lower non-recoverable going high. "},
        {0x06, 0x01, "Crossed Upper non-critical going low. "},
        {0x07, 0x01, "Crossed Upper non-critical going high. "},
        {0x08, 0x02, "Crossed Upper critical going low. "},
        {0x09, 0x02, "Crossed Upper critical going high. "},
        {0x0A, 0x03, "Crossed Upper non-recoverable going low. "},
        {0x0B, 0x03, "Crossed Upper non-recoverable going high. "},
DMS_DECLARE_SEN_TYPE_END;

    /* CURRENT Sensor type */
    DMS_DECLARE_SEN_TYPE_START(0x03)
        {0x00, 0x01, "Crossed Lower non-critical going low. "},
        {0x01, 0x01, "Crossed Lower non-critical going high. "},
        {0x02, 0x02, "Crossed Lower critical going low. "},
        {0x03, 0x02, "Crossed Lower critical going high. "},
        {0x04, 0x03, "Crossed Lower non-recoverable going low. "},
        {0x05, 0x03, "Crossed Lower non-recoverable going high. "},
        {0x06, 0x01, "Crossed Upper non-critical going low. "},
        {0x07, 0x01, "Crossed Upper non-critical going high. "},
        {0x08, 0x02, "Crossed Upper critical going low. "},
        {0x09, 0x02, "Crossed Upper critical going high. "},
        {0x0A, 0x03, "Crossed Upper non-recoverable going low. "},
        {0x0B, 0x03, "Crossed Upper non-recoverable going high. "},
DMS_DECLARE_SEN_TYPE_END;

    /* FAN Sensor type */
    DMS_DECLARE_SEN_TYPE_START(0x04)
        {0x00, 0x01, "Crossed Lower non-critical going low. "},
        {0x01, 0x01, "Crossed Lower non-critical going high. "},
        {0x02, 0x02, "Crossed Lower critical going low. "},
        {0x03, 0x02, "Crossed Lower critical going high. "},
        {0x04, 0x03, "Crossed Lower non-recoverable going low. "},
        {0x05, 0x03, "Crossed Lower non-recoverable going high. "},
        {0x06, 0x01, "Crossed Upper non-critical going low. "},
        {0x07, 0x01, "Crossed Upper non-critical going high. "},
        {0x08, 0x02, "Crossed Upper critical going low. "},
        {0x09, 0x02, "Crossed Upper critical going high. "},
        {0x0A, 0x03, "Crossed Upper non-recoverable going low. "},
        {0x0B, 0x03, "Crossed Upper non-recoverable going high. "},
DMS_DECLARE_SEN_TYPE_END;

    /* PHYSICAL SECURITY Sensor type */
    DMS_DECLARE_SEN_TYPE_START(0x05)
        {0x00, 0x02, "General Chassis Intrusion. "},
        {0x01, 0x02, "Drive Bay intrusion. "},
        {0x02, 0x02, "I/O Card area intrusion. "},
        {0x03, 0x02, "Processor area intrusion. "},
        {0x04, 0x02, "LAN Leash Lost (system is unplugged from LAN). "},
        {0x05, 0x02, "Unauthorized dock/undock. "},
        {0x06, 0x02, "FAN area intrusion (supports detection of hot plug fan tampering). "},
DMS_DECLARE_SEN_TYPE_END;

    /* 6: Platform Security Violation Attempt Sensor,type:06h,offset:00h-05H,all:6 */
    DMS_DECLARE_SEN_TYPE_START(0x06)
        {0x00, 0x02, "Secure Mode (Front Panel Lockout) Violation attempt. "},
        {0x01, 0x02, "Pre-boot Password Violation - user password. "},
        {0x02, 0x02, "Pre-boot Password Violation attempt - setup password. "},
        {0x03, 0x02, "Pre-boot Password Violation - network boot password. "},
        {0x04, 0x02, "Other pre-boot Password Violation. "},
        {0x05, 0x02, "Out-of-band Access Password Violation. "},
DMS_DECLARE_SEN_TYPE_END;

    /* 7: Processor Sensor,type:07H,offset:00h-0Ah,all:11 */
    DMS_DECLARE_SEN_TYPE_START(0x07)
        {0x00, 0x03, "IERR. "},
        {0x01, 0x03, "Thermal Trip. "},
        {0x02, 0x03, "FRB1/BIST failure. "},
        {0x03, 0x03, "FRB2/Hang in POST failure. "},
        {0x04, 0x03, "FRB3/Processor Startup/Initialization failure (CPU didn't start). "},
        {0x05, 0x03, "Configuration Error. "},
        {0x06, 0x03, "SM BIOS 'Uncorrectable CPU-complex Error'. "},
        {0x07, 0x00, "Processor Presence detected. "},
        {0x08, 0x00, "Processor disabled. "},
        {0x09, 0x00, "Terminator Presence Detected. "},
        {0x0A, 0x03, "Processor Automatically Throttled. "},
DMS_DECLARE_SEN_TYPE_END;

    // Power Supply  Sensor,type:08h,offset:00h-06H,all:7
    DMS_DECLARE_SEN_TYPE_START(0x08)
        {0x00, 0x00, "Presence detected. "},
        {0x01, 0x03, "Power Supply Failure detected. "},
        {0x02, 0x03, "Predictive Failure. "},
        {0x03, 0x03, "Power Supply input lost. "},
        {0x04, 0x03, "Power Supply input lost or out-of-range. "},
        {0x05, 0x03, "Power Supply input out-of-range, but present. "},
        {0x06, 0x03, "Configuration error."},
        {0x07, 0x01, "Power Supply output out-of-range, but present. "},
        {0x08, 0x01, "Power Supply output SW control fail."},
DMS_DECLARE_SEN_TYPE_END;

    /* Power_Unit  Sensor,type:09h */
    DMS_DECLARE_SEN_TYPE_START(0x09)
        {0x00, 0x03, "Power Off / Power Down. "},
        {0x01, 0x03, "Power Cycle. "},
        {0x02, 0x03, "240VA Power Down. "},
        {0x03, 0x03, "Interlock Power Down. "},
        {0x04, 0x03, "AC lost. "},
        {0x05, 0x03, "Soft Power Control Failure. "},
        {0x06, 0x03, "Power Unit Failure detected. "},
        {0x07, 0x03, "Predictive Failure. "},
        {0x08, 0x01, "PMU non-critical high temp. "},
        {0x09, 0x02, "PMU temp sensor is bad. "},
        {0x0A, 0x01, "PMU reporting timeout(minor). "},
        {0x0B, 0x02, "PMU config error. "},
        {0x0C, 0x00, "reserved. "},
        {0x0D, 0x02, "Power Unit can not be access. "},
DMS_DECLARE_SEN_TYPE_END;

    /* 0x0c: Memory  Sensor,type:0CH,offset:00H-08h,all:9 */
    DMS_DECLARE_SEN_TYPE_START(0x0C)
        {0x00, 0x00, "Correctable ECC / other correctable memory error. "},
        {0x01, 0x03, "Uncorrectable ECC / other uncorrectable memory error. "},
        {0x02, 0x03, "Parity. "},
        {0x03, 0x03, "Memory Scrub Failed (stuck bit). "},
        {0x04, 0x03, "Memory Device Disabled. "},
        {0x05, 0x01, "Correctable ECC / other correctable memory error logging limit reached. "},
        {0x06, 0x00, "Presence detected. "},
        {0x07, 0x03, "Configuration error. "},
        {0x08, 0x02, "Spare. "},
        {0x09, 0x02, "Scrub Uncorrectable ECC / other uncorrectable memory error. "},
        {0x0A, 0x01, "Scrub Correctable ECC / other correctable memory error logging limit reached. "},
        {0x0B, 0x00, "Link-ECC 1 bit error. "},
        {0x0C, 0x02, "Link-ECC 2 bit error. "},
        {0x0D, 0x00, "Mirror sub error. "},
DMS_DECLARE_SEN_TYPE_END;

    /* 0x0f:  System Firmware Progress(formerly POST Error) Sensor,type:0Fh,offset:00h-02h,all:3 */
    DMS_DECLARE_SEN_TYPE_START(0x0F)
        {0x00, 0x01, "System Firmware Error (POST Error)"},
        {0x01, 0x03, "System Firmware Hang"},
        {0x02, 0x00, "System Firmware Progress"},
        {0x03, 0x02, "System Firmware Error (next fw boot failure)"},
        {0x04, 0x02, "System Firmware Error (next SOC boot failure)"},
DMS_DECLARE_SEN_TYPE_END;

    /* 0x10:  Event  Logging Disabled Sensor,type:10H,offset:00h-04h,all:5 */
    DMS_DECLARE_SEN_TYPE_START(0x10)
        {0x00, 0x02, "Correctable Memory Error Logging Disabled. "},
        {0x01, 0x02, "Event 'Type' Logging Disabled. "},
        {0x02, 0x02, "Log Area Reset/Cleared. "},
        {0x03, 0x02, "All Event Logging Disabled. "},
        {0x04, 0x02, "SEL Full. "},
        {0x05, 0x00, "SEL Not FUll. "},
DMS_DECLARE_SEN_TYPE_END;

    /* 0x11:  Watchdog 1  Sensor,type:11H,offset:00H-07H,all:8 */
    DMS_DECLARE_SEN_TYPE_START(0x11)
        {0x00, 0x02, "BIOS Watchdog Reset. "},
        {0x01, 0x02, "OS Watchdog Reset. "},
        {0x02, 0x02, "OS Watchdog Shut Down. "},
        {0x03, 0x02, "OS Watchdog Power Down. "},
        {0x04, 0x02, "OS Watchdog Power Cycle. "},
        {0x05, 0x00, "OS Watchdog NMI / Diagnostic Interrupt. "},
        {0x06, 0x02, "OS Watchdog Expired, status only. "},
        {0x07, 0x02, "OS Watchdog pre-timeout Interrupt, non-NMI. "},
DMS_DECLARE_SEN_TYPE_END;

    /* 0x12:  System Event Sensor,type:12H,offset:00h-04h,all:5 */
    DMS_DECLARE_SEN_TYPE_START(0x12)
        {0x00, 0x00, "System Reconfigured. "},
        {0x01, 0x00, "OEM System Boot Event. "},
        {0x02, 0x03, "Undetermined system hardware failure. "},
        {0x03, 0x00, "Entry added to Auxiliary Log. "},
        {0x04, 0x00, "PEF Action. "},
DMS_DECLARE_SEN_TYPE_END;

    /* 0x13:  Critical Interrupt Sensor,type:13h,offset:00h-09h,all:10 */
    DMS_DECLARE_SEN_TYPE_START(0x13)
        {0x00, 0x00, "Front Panel NMI / Diagnostic Interrupt. "},
        {0x01, 0x02, "Bus Timeout. "},
        {0x02, 0x00, "I/O channel check NMI. "},
        {0x03, 0x00, "Software NMI. "},
        {0x04, 0x02, "PCI PERR. "},
        {0x05, 0x02, "PCI SERR. "},
        {0x06, 0x02, "EISA Fail Safe Timeout. "},
        {0x07, 0x02, "Bus Correctable Error. "},
        {0x08, 0x02, "Bus Uncorrectable Error. "},
        {0x09, 0x02, "Critical NMI (port 61h, bit 7). "},
DMS_DECLARE_SEN_TYPE_END;

    /* 14:   Button /  Switch Sensor,type:14h,offset:00h-04h,all:5 */
    DMS_DECLARE_SEN_TYPE_START(0x14)
        {0x00, 0x00, "Power Button pressed. "},
        {0x01, 0x00, "Sleep Button pressed. "},
        {0x02, 0x00, "Reset Button pressed. "},
        {0x03, 0x02, "FRU latch open. "},
        {0x04, 0x00, "FRU service request button. "},
        {0x05, 0x00, "FRU latch close. "},
DMS_DECLARE_SEN_TYPE_END;

    /* System Boot Initiated Sensor,type:1Dh,offset:00h-04h,all:5 */
    DMS_DECLARE_SEN_TYPE_START(0x1D)
        {0x00, 0x00, "Initiated by power up. "},
        {0x01, 0x00, "Initiated by hard reset. "},
        {0x02, 0x00, "Initiated by warm reset. "},
        {0x03, 0x00, "User requested PXE boot. "},
        {0x04, 0x00, "Automatic boot to diagnostic. "},
        {0x05, 0x00, "OS/run-time software initiated hard reset. "},
        {0x06, 0x00, "OS/run-time software initiated warm reset. "},
        {0x07, 0x00, "System restart"},
DMS_DECLARE_SEN_TYPE_END;

    /* Boot  Error  Sensor,type:1Eh,offset:00h-04,all:5 */
    DMS_DECLARE_SEN_TYPE_START(0x1E)
        {0x00, 0x03, "No bootable media. "},
        {0x01, 0x03, "Non-bootable diskette left in drive. "},
        {0x02, 0x03, "PXE Server not found. "},
        {0x03, 0x03, "Invalid boot sector. "},
        {0x04, 0x03, "Timeout waiting for user selection of boot source. "},
DMS_DECLARE_SEN_TYPE_END;

    // 18: OS Boot Sensor,type:1Fh,offset:00h-06h,all:7
    DMS_DECLARE_SEN_TYPE_START(0x1F)
        {0x00, 0x00, "A: boot completed. "},
        {0x01, 0x00, "C: boot completed. "},
        {0x02, 0x00, "PXE boot completed. "},
        {0x03, 0x00, "Diagnostic boot completed. "},
        {0x04, 0x00, "CD-ROM boot completed. "},
        {0x05, 0x00, "ROM boot completed. "},
        {0x06, 0x00, "Boot completed - boot device not specified. "},
DMS_DECLARE_SEN_TYPE_END;

    /* OS Critical Stop Sensor,type:20h,00h-01h,all:2 */
    DMS_DECLARE_SEN_TYPE_START(0x20)
        {0x00, 0x03, "Stop during OS load / initialization. "},
        {0x01, 0x03, "Run-time Stop. "},
DMS_DECLARE_SEN_TYPE_END;

    /* Slot  / Connector Sensor(Sensor Type),type:21h,offset:00h-09h,all:10 */
    DMS_DECLARE_SEN_TYPE_START(0x21)
        {0x00, 0x01, "Sensor Status asserted. "},
        {0x01, 0x00, "Identify Status asserted. "},
        {0x02, 0x00, "Slot / Connector Device installed/attached. "},
        {0x03, 0x00, "Slot / Connector Ready for Device Installation. "},
        {0x04, 0x00, "Slot/Connector Ready for Device Removal. "},
        {0x05, 0x00, "Slot Power is Off. "},
        {0x06, 0x00, "Slot / Connector Device Removal Request. "},
        {0x07, 0x03, "Interlock asserted. "},
        {0x08, 0x03, "Slot is Disabled. "},
        {0x09, 0x00, "Slot holds spare device. "},
DMS_DECLARE_SEN_TYPE_END;

    /* 21: System  ACPI  Power State Sensor(Sensor Type),type:22h,offset:00-0Eh,all:14 */
    DMS_DECLARE_SEN_TYPE_START(0x22)
        {0x00, 0x00, "S0 / G0 'working'. "},
        {0x01, 0x00, "S1 'sleeping with system h/w & processor context maintained'. "},
        {0x02, 0x03, "S2 'sleeping, processor context lost'. "},
        {0x03, 0x00, "S3 'sleeping, processor & h/w context lost, memory retained.'. "},
        {0x04, 0x00, "S4 'non-volatiles sleep / suspend-to disk'. "},
        {0x05, 0x00, "S5 / G2 'soft-off'. "},
        {0x06, 0x00, "S4 / S5 soft-off, particular S4 / S5 state cannot be determined. "},
        {0x07, 0x00, "G3 / Mechanical Off. "},
        {0x08, 0x03, "Sleeping in an S1, S2, or S3 states. "},
        {0x09, 0x03, "G1 sleeping (S1-S4 state cannot be determined). "},
        {0x0A, 0x00, "S5 entered by override. "},
        {0x0B, 0x00, "Legacy ON state. "},
        {0x0C, 0x00, "Legacy OFF state. "},
DMS_DECLARE_SEN_TYPE_END;

    /* Watch dog 2  Sensor,type:23h,offset:00-04h,all:5 */
    DMS_DECLARE_SEN_TYPE_START(0x23)
        {0x00, 0x03, "Timer expired, status only (no action, no interrupt). "},
        {0x01, 0x03, "Hard Reset. "},
        {0x02, 0x03, "Power Down. "},
        {0x03, 0x03, "Power Cycle. "},
        {0x04, 0x00, "reserved. "},
        {0x05, 0x00, "reserved. "},
        {0x06, 0x00, "reserved. "},
        {0x07, 0x00, "reserved. "},
        {0x08, 0x03, "Timer interrupt. "},
DMS_DECLARE_SEN_TYPE_END;

    // 23:  Platform Alert Sensor,type:24h,offset:00-03,all:4
    DMS_DECLARE_SEN_TYPE_START(0x24)
        {0x00, 0x00, "Platform generated page. "},
        {0x01, 0x00, "Platform generated LAN alert. "},
        {0x02, 0x00, "Platform Event Trap generated, formatted per IPMI PET specification. "},
        {0x03, 0x00, "Platform generated SNMP trap, OEM format. "},
DMS_DECLARE_SEN_TYPE_END;

    /* Entity Presence Sensor,type:25h,offset:00-02,all:3 */
    DMS_DECLARE_SEN_TYPE_START(0x25)
        {0x00, 0x00, "Entity Present. "},
        {0x01, 0x03, "Entity Absent. "},
        {0x02, 0x03, "Entity Disabled. "},
DMS_DECLARE_SEN_TYPE_END;

    /* Monitor ASIC/IC sensor */
    DMS_DECLARE_SEN_TYPE_START(0x26)
        {0x00, 0x00, "Monitor ASIC/IC is ok. "},
        {0x01, 0x02, "Monitor ASIC/IC is bad. "},
DMS_DECLARE_SEN_TYPE_END;

    /* HEARTBEAT sensor ,type:27h,offset:00-01,all:2 */
    DMS_DECLARE_SEN_TYPE_START(0x27)
        {0x00, 0x02, "Heartbeat Lost."},
        {0x01, 0x02, "Heartbeat Lost(2nd Monitor Center)."},
DMS_DECLARE_SEN_TYPE_END;


    /* Management  Subsystem Health Sensor,type:28h,offset:00-03h,all:4 */
    DMS_DECLARE_SEN_TYPE_START(0x28)
        {0x00, 0x02, "Sensor access degraded or unavailable. "},
        {0x01, 0x02, "Controller access degraded or unavailable. "},
        {0x02, 0x02, "Management controller off-line. "},
        {0x03, 0x02, "Management controller unavailable. "},
DMS_DECLARE_SEN_TYPE_END;


    /* Battery Sensor,type:29h,offset:00h-02h,all:3 */
    DMS_DECLARE_SEN_TYPE_START(0x29)
        {0x00, 0x02, "Battery low (predictive failure). "},
        {0x01, 0x02, "Battery failed. "},
        {0x02, 0x00, "Battery presence detected. "},
DMS_DECLARE_SEN_TYPE_END;

    /* Session Audit Sensor,type:2Ah,offset:00h-01h,all:2 */
    DMS_DECLARE_SEN_TYPE_START(0x2A)
        {0x00, 0x00, "Session Activated. "},
        {0x01, 0x00, "Session Deactivated. "},
DMS_DECLARE_SEN_TYPE_END;

    /* Version  Change Sensor,type:2B,offset:00h-07h,all:8 */
    DMS_DECLARE_SEN_TYPE_START(0x2B)
        {0x00, 0x02, "Hardware change detected with associated Entity. "},
        {0x01, 0x02, "Firmware or software change detected with associated Entity. "},
        {0x02, 0x00, "Hardware incompatibility detected with associated Entity. "},
        {0x03, 0x00, "Firmware or software incompatibility detected with associated Entity. "},
        {0x04, 0x00, "Entity is of an invalid or unsupported hardware version. "},
        {0x05, 0x00, "Entity contains an invalid or unsupported firmware or software version. "},
        {0x06, 0x02, "Hardware Change detected with associated Entity was successful. "},
        {0x07, 0x02, "Software or F/W Change detected with associated Entity was successful. "},
DMS_DECLARE_SEN_TYPE_END;

    /* SOC RAS Sensor,type:C0 */
    DMS_DECLARE_SEN_TYPE_START(0xC0)
        {0x00, 0x02, "module error"},
        {0x01, 0x03, "module error can not be fixed"},
        {0x02, 0x02, "input error"},
        {0x03, 0x02, "internal config error"},
        {0x04, 0x02, "config error"},
        {0x05, 0x02, "parity error"},
        {0x06, 0x01, "single bit ecc error exceeding the threshold"},
        {0x07, 0x02, "single bit ecc error not corrected"},
        {0x08, 0x02, "multiple bit ecc error"},
        {0x09, 0x02, "bus error, probably caused by software"},
        {0x0a, 0x02, "service timeout"},
        {0x0b, 0x02, "heartbeat timeout"},
        {0x0c, 0x02, "module internal err"},
        {0x0d, 0x00, "link error"},
        {0x0e, 0x01, "multi bit ecc error can be corrected"},
        {0x0f, 0x00, "dq parity error"},
DMS_DECLARE_SEN_TYPE_END;

    /* Authorization Expiration Sensor,type:C1,offset:00h-01h,all:2 */
    DMS_DECLARE_SEN_TYPE_START(0xC1)
        {0x00, 0x01, "About to expire. "},
        {0x01, 0x02, "Has expired. "},
        {0x02, 0x01, "Crl is about to expire. "},
        {0x03, 0x02, "Crl has expired. "},
        {0x04, 0x01, "CA cert is about to expire. "},
        {0x05, 0x02, "CA cert has expired. "},
DMS_DECLARE_SEN_TYPE_END;

/* Memory Error Record sensor */
    DMS_DECLARE_SEN_TYPE_START(0xC2)
        {0x00, 0x01, "New uncorrectable ECC / other uncorrectable memory error."},
        {0x01, 0x01, "Uncorrectable ECC address count reached normal threshold."},
        {0x02, 0x03, "Uncorrectable ECC address count reached the upper limits."},
        {0x03, 0x03, "single bit ecc error exceeding the threshold(worse)."},
        {0x04, 0x02, "Memory UCE Interrupt mask."},
		{0x05, 0x01, "Memory in sub-health"},
DMS_DECLARE_SEN_TYPE_END;

    /* SOC Safety Sensor,type:C3 */
    DMS_DECLARE_SEN_TYPE_START(0xC3)
        { 0x00, 3,  "lockstep error" },
        { 0x01, 2,  "register overflow fault" },
        { 0x02, 2,  "port tx timeout" },
        { 0x03, 2,  "port link status change(up to down)" },
        { 0x04, 1,  "port config error" },
        { 0x05, 1,  "port lane drop" },
        { 0x06, 0,  "undefined" },
        { 0x07, 1,  "slave port link status change(up to down)" },
        { 0x08, 0,  "port retry timeout exception error" },
        { 0x09, 0,  "op retry success" },
        { 0x0A, 1,  "use backup link" },
        { 0x0B, 1,  "op retry fail" },
        { 0x0C, 2,  "channel error" },
        { 0x0D, 2,  "read data poison" },
        { 0x0E, 1,  "CPU wakeup failure notification" },
        { 0x0F, 1,  "unauthorized access failure notification" },
DMS_DECLARE_SEN_TYPE_END;

    /* SOC Extend Sensor,type:C4 */
    DMS_DECLARE_SEN_TYPE_START(0xC4)
        { 0x00, 1,  "monitor timeout(minor)" },
        { 0x01, 2,  "monitor timeout(major)" },
        { 0x02, 1,  "reporting timeout(minor)" },
        { 0x03, 2,  "reporting timeout(major)" },
        { 0x04, 2,  "reporting channel error" },
        { 0x05, 2,  "reporting channel error (2nd Monitor Center)" },
        { 0x06, 2,  "register error(read after write)" },
        { 0x07, 2,  "register error(period read)" },
        { 0x08, 1,  "register error without service affaction(period read)" },
        { 0x09, 2,  "STL test error" },
        { 0x0A, 2,  "STL diagnostics do not work"},
        { 0x0B, 0,  "reporting channel selfcheck ok"},
        { 0x0C, 0,  "Link diagnosis exception"},
        { 0x0D, 2,  "Interruption storm exception"},
DMS_DECLARE_SEN_TYPE_END;

    /* BUS Sensor,type:C5 */
    DMS_DECLARE_SEN_TYPE_START(0xC5)
        { 0x00, 1,  "bus's timestamp is abnormal" },
        { 0x01, 2,  "TX/RX events are lost" },
        { 0x02, 1,  "bus overload" },
        { 0x03, 2,  "bus bus-off state" },
DMS_DECLARE_SEN_TYPE_END;

    /* Check Sensor,type:C6 */
    DMS_DECLARE_SEN_TYPE_START(0xC6)
        { 0x00, 2,  "check fail" },
        { 0x01, 2,  "CRC check fail" },
        { 0x02, 2,  "RTSQ_check_fail" },
        { 0x03, 2,  "ACSQ_check_fail" },
        { 0x04, 2,  "PCIE_DMA_check_fail" },
        { 0x05, 2,  "stress check fail" },
        { 0x06, 2,  "stress check abnormal" },
DMS_DECLARE_SEN_TYPE_END;

    /* Crypto Sensor,type:C7 */
    DMS_DECLARE_SEN_TYPE_START(0xC7)
        { 0x00, 2,  "system-level security mechanism failure" },
        { 0x01, 2,  "module-level security mechanism failure" },
        { 0x02, 2,  "crypto algorithm module failure" },
        { 0x03, 2,  "KeyMgr module failure" },
        { 0x04, 2,  "TRNG module failure" },
        { 0x05, 2,  "attack defense module failure" },
        { 0x06, 2,  "EFUSE cannot be read" },
        { 0x07, 2,  "EFUSE cannot be burnt" },
        { 0x08, 2,  "TEEOS system time error" },
        { 0x09, 2,  "SEC module error" },
        { 0x0A, 2,  "HPRE module error" },
        { 0x0B, 2,  "Cryptographic Subsystem totally failure" },
        { 0x0C, 2,  "Cryptographic operation error" },
        { 0x0D, 2,  "Cryptographic key management error" },
        { 0x0E, 2,  "Cryptographic RNG error" },
DMS_DECLARE_SEN_TYPE_END;

    /* AA Sensor,type:C8 */
    DMS_DECLARE_SEN_TYPE_START(0xC8)
        { 0x00, 1,  " " },
DMS_DECLARE_SEN_TYPE_END;

    /* Scheduler Sensor,type:C9 */
    DMS_DECLARE_SEN_TYPE_START(0xC9)
        { 0x00, 2, "Tsensor signal timeout "},
        { 0x01, 2, "MBIGEN lockstep error "},
DMS_DECLARE_SEN_TYPE_END;

    /* Dispatch Sensor,type:CA */
    DMS_DECLARE_SEN_TYPE_START(0xCA)
        { 0x00, 1,  " " },
DMS_DECLARE_SEN_TYPE_END;

    /* SMMU Sensor,type:CB */
    DMS_DECLARE_SEN_TYPE_START(0xCB)
        { 0x00, 1,  " " },
DMS_DECLARE_SEN_TYPE_END;

    /* Dispatch Sensor,type:D0 */
    DMS_DECLARE_SEN_TYPE_START(0xD0)
        { 0x00, 2,  "The process start failed or exit abnormally" },
        { 0x01, 0,  "memory over limit" },
        { 0x02, 2,  "ext4_handle_error Recoverable Error" },
        { 0x03, 2,  "ext4_abort Non-recoverable Error" },
        { 0x04, 2,  "dm-verify check failed" },
        { 0x05, 1,  "normal software resource recycle failed" },
        { 0x06, 2,  "critic software resource recycle failed" },
        { 0x07, 2,  "The monitoring function is abnormal" },
        { 0x08, 2,  "The system scheduling is abnormal." },
        { 0x09, 0,  "OS init" },
        { 0x0A, 3,  "SOC Lost" },
        { 0x0B, 2,  "system memory used by service threshold over limit" },
        { 0x0C, 2,  "The software fails to cannot recover."},
DMS_DECLARE_SEN_TYPE_END;

    /* Chip hardware,type:D1 */
    DMS_DECLARE_SEN_TYPE_START(0xD1)
        { 0x00, 0,  "Read profile tops from flash failed" },
        { 0x01, 2,  "The voltage adjustment function is abnormal" },
        { 0x02, 2,  "The freq adjustment function is abnormal" },
        { 0x03, 2,  "The function of obtaining the current is abnormal" },
        { 0x04, 2,  "resevered" },
        { 0x05, 2,  "resevered" },
        { 0x06, 2,  "resevered" },
        { 0x07, 2,  "LPM module internal error" },
DMS_DECLARE_SEN_TYPE_END;

    /* Note: that the sensor type name must be less than 20 bytes */
static struct tag_dms_sensor_type g_dms_sensor_type[] = {
    DMS_ADD_SENSOR_TYPE(0x01, "Temperature"),
    DMS_ADD_SENSOR_TYPE(0x02, "Voltage"),
    DMS_ADD_SENSOR_TYPE(0x03, "Current"),
    DMS_ADD_SENSOR_TYPE(0x04, "Fan"),
    DMS_ADD_SENSOR_TYPE(0x05, "Physical Secur"),
    DMS_ADD_SENSOR_TYPE(0x06, "Plat Security"),
    DMS_ADD_SENSOR_TYPE(0x07, "Processor"),
    DMS_ADD_SENSOR_TYPE(0x08, "Power Supply"),
    DMS_ADD_SENSOR_TYPE(0x09, "Power"),
    DMS_ADD_SENSOR_TYPE(0x0C, "Memory"),
    DMS_ADD_SENSOR_TYPE(0x0F, "System Firm Prog"),
    DMS_ADD_SENSOR_TYPE(0x10, "Log Disabled"),
    DMS_ADD_SENSOR_TYPE(0x11, "Watchdog 1"),
    DMS_ADD_SENSOR_TYPE(0x12, "System Event"),
    DMS_ADD_SENSOR_TYPE(0x13, "Criti Interrupt"),
    DMS_ADD_SENSOR_TYPE(0x14, "Button / Switch"),
    DMS_ADD_SENSOR_TYPE(0x1D, "System Boot Init"),
    DMS_ADD_SENSOR_TYPE(0x1E, "Boot Error"),
    DMS_ADD_SENSOR_TYPE(0x1F, "OS Boot"),
    DMS_ADD_SENSOR_TYPE(0x20, "OS Critical Stop"),
    DMS_ADD_SENSOR_TYPE(0x21, "Slot / Connector"),
    DMS_ADD_SENSOR_TYPE(0x22, "ACPI Power State"),
    DMS_ADD_SENSOR_TYPE(0x23, "Watchdog"),
    DMS_ADD_SENSOR_TYPE(0x24, "Platform Alert"),
    DMS_ADD_SENSOR_TYPE(0x25, "Entity Presence"),
    DMS_ADD_SENSOR_TYPE(0x26, "Monitor ASIC/IC"),
    DMS_ADD_SENSOR_TYPE(0x27, "Heartbeat Sensor"),
    DMS_ADD_SENSOR_TYPE(0x28, "Manage Sub Heal"),
    DMS_ADD_SENSOR_TYPE(0x29, "Battery"),
    DMS_ADD_SENSOR_TYPE(0x2A, "Session Audit"),
    DMS_ADD_SENSOR_TYPE(0x2B, "Version Change"),
    DMS_ADD_SENSOR_TYPE(0xC0, "RAS State"),
    DMS_ADD_SENSOR_TYPE(0xC1, "Expiration Status"),
    DMS_ADD_SENSOR_TYPE(0xC2, "Memory Err Rec"),
    DMS_ADD_SENSOR_TYPE(0xC3, "Safety State"),
    DMS_ADD_SENSOR_TYPE(0xC4, "Extend Sensor"),
    DMS_ADD_SENSOR_TYPE(0xC5, "Bus Sensor"),
    DMS_ADD_SENSOR_TYPE(0xC6, "Check Sensor"),
    DMS_ADD_SENSOR_TYPE(0xC7, "Crypto Sensor"),
    DMS_ADD_SENSOR_TYPE(0xC8, "AA Sensor"),
    DMS_ADD_SENSOR_TYPE(0xC9, "Scheduler Sensor"),
    DMS_ADD_SENSOR_TYPE(0xCA, "Dispatch Sensor"),
    DMS_ADD_SENSOR_TYPE(0xCB, "SMMU Sensor"),
    DMS_ADD_SENSOR_TYPE(0xD0, "General Software"),
    DMS_ADD_SENSOR_TYPE(0xD1, "Chip Hardware")
};

int dms_get_sensor_type_count(void)
{
    return sizeof(g_dms_sensor_type) / sizeof(struct tag_dms_sensor_type);
}
struct tag_dms_sensor_type* dms_get_sensor_type(void)
{
    return g_dms_sensor_type;
}
