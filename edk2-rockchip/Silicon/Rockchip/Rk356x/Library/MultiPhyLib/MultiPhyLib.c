/** @file
 *
 *  RK3566/RK3568 MULTI-PHY Library.
 * 
 *  Copyright (c) 2021, Jared McNeill <jmcneill@invisible.ca>
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/CruLib.h>
#include <Library/MultiPhyLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <IndustryStandard/Rk356x.h>

#define MULTIPHY_REGISTER(RegNo)    (((RegNo) - 1) * 4)

#define PIPE_PHY_GRF_PIPE_CON0      0x0000
#define  PIPE_DATABUSWIDTH_MASK     0x3
#define  PIPE_DATABUSWIDTH_16BIT    0x1
#define  PIPE_PHYMODE_SHIFT         2
#define  PIPE_PHYMODE_MASK          (0x3U << PIPE_PHYMODE_SHIFT)
#define  PIPE_PHYMODE_USB3          (1U << PIPE_PHYMODE_SHIFT)
#define  PIPE_RATE_SHIFT            4
#define  PIPE_RATE_MASK             (0x3U << PIPE_RATE_SHIFT)
#define  PIPE_RATE_USB3_5GBPS       (0U << PIPE_RATE_SHIFT)
#define PIPE_PHY_GRF_PIPE_CON1      0x0004
#define  PHY_CLK_SEL_SHIFT          13
#define  PHY_CLK_SEL_MASK           (0x3U << PHY_CLK_SEL_SHIFT)
#define  PHY_CLK_SEL_24M            (0U << PHY_CLK_SEL_SHIFT)
#define  PHY_CLK_SEL_25M            (1U << PHY_CLK_SEL_SHIFT)
#define  PHY_CLK_SEL_100M           (2U << PHY_CLK_SEL_SHIFT)
#define PIPE_PHY_GRF_PIPE_CON2      0x0008
#define  SEL_PIPE_TXCOMPLIANCE_I    (1U << 15)
#define  SEL_PIPE_TXELECIDLE        (1U << 12)
#define PIPE_PHY_GRF_PIPE_CON3      0x000c
#define  PIPE_SEL_SHIFT             13
#define  PIPE_SEL_MASK              (0x3U << PIPE_SEL_SHIFT)
#define  PIPE_SEL_USB3              (1U << PIPE_SEL_SHIFT)   /* pipephy0/1 */
#define PIPE_PHY_GRF_PIPE_STATUS1   0x0034
#define  PIPE_PHYSTATUS_O           (1U << 6)

#define SOFTRST_INDEX               28
#define SOFTRST_BIT(n)              (5 + (n) * 2)

STATIC
VOID
GrfUpdateRegister (
  IN EFI_PHYSICAL_ADDRESS Reg,
  IN UINT32 Mask,
  IN UINT32 Val
  )
{
  MmioWrite32 (Reg, (Mask << 16) | Val);
}

STATIC
EFI_STATUS
MultiPhySetModeUsb3 (
  IN UINT32 Index
  )
{
    EFI_PHYSICAL_ADDRESS BaseAddr = PIPE_PHY (Index);
    EFI_PHYSICAL_ADDRESS PhyGrfBaseAddr = PIPE_PHY_GRF (Index);
    UINT32 PhyClkSel;
    UINTN Rate;

    /* USB3 is only supported on pipephy0 and pipephy1 */
    ASSERT (Index == 0 || Index == 1);

    /* Set SSC down-spread spectrum */
    MmioAndThenOr32 (BaseAddr + MULTIPHY_REGISTER (32), ~0x30, 0x10);

    /* Enable adaptive CTLE signal */
    MmioOr32 (BaseAddr + MULTIPHY_REGISTER (15), 0x1);

    /* Connect PHY to USB3 controller */
    GrfUpdateRegister (PhyGrfBaseAddr + PIPE_PHY_GRF_PIPE_CON3,
                       PIPE_SEL_MASK,
                       PIPE_SEL_USB3);

    /* Pipe TX source selection */
    GrfUpdateRegister (PhyGrfBaseAddr + PIPE_PHY_GRF_PIPE_CON2,
                       SEL_PIPE_TXCOMPLIANCE_I | SEL_PIPE_TXELECIDLE,
                       0);

    /* Enable USB3 PHY mode, 16-bit data bus, 5Gbps */
    GrfUpdateRegister (PhyGrfBaseAddr + PIPE_PHY_GRF_PIPE_CON0,
                       PIPE_DATABUSWIDTH_MASK  | PIPE_PHYMODE_MASK | PIPE_RATE_MASK,
                       PIPE_DATABUSWIDTH_16BIT | PIPE_PHYMODE_USB3 | PIPE_RATE_USB3_5GBPS);

    /* Clock setup */
    Rate = CruGetPciePhyClockRate (Index);
    DEBUG ((DEBUG_INFO, "MultiPhySetModeUsb3(%u, ...): Rate = %lu Hz\n", Index, Rate));
    if (Rate == 24000000) {
        PhyClkSel = PHY_CLK_SEL_24M;

        /* Set PLL control SSC module period to 31.5 kHz */
        MmioAndThenOr32 (BaseAddr + MULTIPHY_REGISTER (15), ~0xc0, 0x40);   /* SSC_CNT[1:0] */
        MmioAndThenOr32 (BaseAddr + MULTIPHY_REGISTER (16), ~0xff, 0x5f);   /* SSC_CNT[9:2] */
    } else if (Rate == 25000000) {
        PhyClkSel = PHY_CLK_SEL_25M;
    } else if (Rate == 100000000) {
        PhyClkSel = PHY_CLK_SEL_100M;
    } else {
        ASSERT (FALSE);
        return EFI_DEVICE_ERROR;
    }

    /* Set PHY reference clock rate */
    GrfUpdateRegister (PhyGrfBaseAddr + PIPE_PHY_GRF_PIPE_CON1,
                       PHY_CLK_SEL_MASK,
                       PhyClkSel);

    return EFI_SUCCESS;
}

EFI_STATUS
MultiPhySetMode (
  IN UINT8 Index,
  IN MULTIPHY_MODE Mode
  )
{
    EFI_STATUS Status;

    ASSERT (Index <= 2);

    switch (Mode) {
    case MULTIPHY_MODE_USB3:
        Status = MultiPhySetModeUsb3 (Index);
        if (Status != EFI_SUCCESS) {
            return Status;
        }
        break;
    default:
        ASSERT (FALSE);
        return EFI_UNSUPPORTED;
    }

    /* De-assert reset */
    CruDeassertSoftReset (SOFTRST_INDEX, SOFTRST_BIT (Index));

    return EFI_SUCCESS;
}