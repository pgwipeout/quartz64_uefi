#
# Copyright (c) 2021, Jared McNeill <jmcneill@invisible.ca>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#

#
# Board specific initialization
#
INF Platform/Pine64/Quartz64/Drivers/BoardInitDxe/BoardInitDxe.inf

#
# Devicetree support
#
INF Platform/Pine64/Quartz64/Drivers/FdtDxe/FdtDxe.inf

#
# ACPI Support
#
INF MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
INF Platform/Pine64/Quartz64/Drivers/PlatformAcpiDxe/PlatformAcpiDxe.inf
INF RuleOverride = ACPITABLE Platform/Pine64/Quartz64/AcpiTables/AcpiTables.inf

#
# SMBIOS Support
#
INF Platform/Pine64/Quartz64/Drivers/PlatformSmbiosDxe/PlatformSmbiosDxe.inf
INF MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf