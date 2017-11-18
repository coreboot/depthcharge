/*
 * Copyright (C) 2018 Google Inc.
 * Copyright (C) 2018 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_GPIO_APL_DEFS_H__
#define __DRIVERS_GPIO_APL_DEFS_H__

/* North community pads */
#define GPIO_0                          0
#define GPIO_1                          1
#define GPIO_2                          2
#define GPIO_3                          3
#define GPIO_4                          4
#define GPIO_5                          5
#define GPIO_6                          6
#define GPIO_7                          7
#define GPIO_8                          8
#define GPIO_9                          9
#define GPIO_10                         10
#define GPIO_11                         11
#define GPIO_12                         12
#define GPIO_13                         13
#define GPIO_14                         14
#define GPIO_15                         15
#define GPIO_16                         16
#define GPIO_17                         17
#define GPIO_18                         18
#define GPIO_19                         19
#define GPIO_20                         20
#define GPIO_21                         21
#define GPIO_22                         22
#define GPIO_23                         23
#define GPIO_24                         24
#define GPIO_25                         25
#define GPIO_26                         26
#define GPIO_27                         27
#define GPIO_28                         28
#define GPIO_29                         29
#define GPIO_30                         30
#define GPIO_31                         31
#define GPIO_32                         32
#define GPIO_33                         33
#define GPIO_34                         34
#define GPIO_35                         35
#define GPIO_36                         36
#define GPIO_37                         37
#define GPIO_38                         38
#define GPIO_39                         39
#define GPIO_40                         40
#define GPIO_41                         41
#define GPIO_42                         42
#define GPIO_43                         43
#define GPIO_44                         44
#define GPIO_45                         45
#define GPIO_46                         46
#define GPIO_47                         47
#define GPIO_48                         48
#define GPIO_49                         49
#define GPIO_62                         50
#define GPIO_63                         51
#define GPIO_64                         52
#define GPIO_65                         53
#define GPIO_66                         54
#define GPIO_67                         55
#define GPIO_68                         56
#define GPIO_69                         57
#define GPIO_70                         58
#define GPIO_71                         59
#define GPIO_72                         60
#define GPIO_73                         61
#define TCK                             62
#define TRST_B                          63
#define TMS                             64
#define TDI                             65
#define CX_PMODE                        66
#define CX_PREQ_B                       67
#define JTAGX                           68
#define CX_PRDY_B                       69
#define TDO                             70
#define CNV_BRI_DT                      71
#define CNV_BRI_RSP                     72
#define CNV_RGI_DT                      73
#define CNV_RGI_RSP                     74
#define SVID0_ALERT_B                   75
#define SVID0_DATA                      76
#define SVID0_CLK                       77

/* North community pads */
#define NW_OFFSET                       78
#define GPIO_187                        NW_OFFSET + 0
#define GPIO_188                        NW_OFFSET + 1
#define GPIO_189                        NW_OFFSET + 2
#define GPIO_190                        NW_OFFSET + 3
#define GPIO_191                        NW_OFFSET + 4
#define GPIO_192                        NW_OFFSET + 5
#define GPIO_193                        NW_OFFSET + 6
#define GPIO_194                        NW_OFFSET + 7
#define GPIO_195                        NW_OFFSET + 8
#define GPIO_196                        NW_OFFSET + 9
#define GPIO_197                        NW_OFFSET + 10
#define GPIO_198                        NW_OFFSET + 11
#define GPIO_199                        NW_OFFSET + 12
#define GPIO_200                        NW_OFFSET + 13
#define GPIO_201                        NW_OFFSET + 14
#define GPIO_202                        NW_OFFSET + 15
#define GPIO_203                        NW_OFFSET + 16
#define GPIO_204                        NW_OFFSET + 17
#define PMC_SPI_FS0                     NW_OFFSET + 18
#define PMC_SPI_FS1                     NW_OFFSET + 19
#define PMC_SPI_FS2                     NW_OFFSET + 20
#define PMC_SPI_RXD                     NW_OFFSET + 21
#define PMC_SPI_TXD                     NW_OFFSET + 22
#define PMC_SPI_CLK                     NW_OFFSET + 23
#define PMIC_PWRGOOD                    NW_OFFSET + 24
#define PMIC_RESET_B                    NW_OFFSET + 25
#define GPIO_213                        NW_OFFSET + 26
#define GPIO_214                        NW_OFFSET + 27
#define GPIO_215                        NW_OFFSET + 28
#define PMIC_THERMTRIP_B                NW_OFFSET + 29
#define PMIC_STDBY                      NW_OFFSET + 30
#define PROCHOT_B                       NW_OFFSET + 31
#define PMIC_I2C_SCL                    NW_OFFSET + 32
#define PMIC_I2C_SDA                    NW_OFFSET + 33
#define GPIO_74                         NW_OFFSET + 34
#define GPIO_75                         NW_OFFSET + 35
#define GPIO_76                         NW_OFFSET + 36
#define GPIO_77                         NW_OFFSET + 37
#define GPIO_78                         NW_OFFSET + 38
#define GPIO_79                         NW_OFFSET + 39
#define GPIO_80                         NW_OFFSET + 40
#define GPIO_81                         NW_OFFSET + 41
#define GPIO_82                         NW_OFFSET + 42
#define GPIO_83                         NW_OFFSET + 43
#define GPIO_84                         NW_OFFSET + 44
#define GPIO_85                         NW_OFFSET + 45
#define GPIO_86                         NW_OFFSET + 46
#define GPIO_87                         NW_OFFSET + 47
#define GPIO_88                         NW_OFFSET + 48
#define GPIO_89                         NW_OFFSET + 49
#define GPIO_90                         NW_OFFSET + 50
#define GPIO_91                         NW_OFFSET + 51
#define GPIO_92                         NW_OFFSET + 52
#define GPIO_97                         NW_OFFSET + 53
#define GPIO_98                         NW_OFFSET + 54
#define GPIO_99                         NW_OFFSET + 55
#define GPIO_100                        NW_OFFSET + 56
#define GPIO_101                        NW_OFFSET + 57
#define GPIO_102                        NW_OFFSET + 58
#define GPIO_103                        NW_OFFSET + 59
#define FST_SPI_CLK_FB                  NW_OFFSET + 60
#define GPIO_104                        NW_OFFSET + 61
#define GPIO_105                        NW_OFFSET + 62
#define GPIO_106                        NW_OFFSET + 63
#define GPIO_109                        NW_OFFSET + 64
#define GPIO_110                        NW_OFFSET + 65
#define GPIO_111                        NW_OFFSET + 66
#define GPIO_112                        NW_OFFSET + 67
#define GPIO_113                        NW_OFFSET + 68
#define GPIO_116                        NW_OFFSET + 69
#define GPIO_117                        NW_OFFSET + 70
#define GPIO_118                        NW_OFFSET + 71
#define GPIO_119                        NW_OFFSET + 72
#define GPIO_120                        NW_OFFSET + 73
#define GPIO_121                        NW_OFFSET + 74
#define GPIO_122                        NW_OFFSET + 75
#define GPIO_123                        NW_OFFSET + 76

/* West community pads */
#define W_OFFSET                        (NW_OFFSET + 77)
#define GPIO_124                        W_OFFSET + 0
#define GPIO_125                        W_OFFSET + 1
#define GPIO_126                        W_OFFSET + 2
#define GPIO_127                        W_OFFSET + 3
#define GPIO_128                        W_OFFSET + 4
#define GPIO_129                        W_OFFSET + 5
#define GPIO_130                        W_OFFSET + 6
#define GPIO_131                        W_OFFSET + 7
#define GPIO_132                        W_OFFSET + 8
#define GPIO_133                        W_OFFSET + 9
#define GPIO_134                        W_OFFSET + 10
#define GPIO_135                        W_OFFSET + 11
#define GPIO_136                        W_OFFSET + 12
#define GPIO_137                        W_OFFSET + 13
#define GPIO_138                        W_OFFSET + 14
#define GPIO_139                        W_OFFSET + 15
#define GPIO_146                        W_OFFSET + 16
#define GPIO_147                        W_OFFSET + 17
#define GPIO_148                        W_OFFSET + 18
#define GPIO_149                        W_OFFSET + 19
#define GPIO_150                        W_OFFSET + 20
#define GPIO_151                        W_OFFSET + 21
#define GPIO_152                        W_OFFSET + 22
#define GPIO_153                        W_OFFSET + 23
#define GPIO_154                        W_OFFSET + 24
#define GPIO_155                        W_OFFSET + 25
#define GPIO_209                        W_OFFSET + 26
#define GPIO_210                        W_OFFSET + 27
#define GPIO_211                        W_OFFSET + 28
#define GPIO_212                        W_OFFSET + 29
#define OSC_CLK_OUT_0                   W_OFFSET + 30
#define OSC_CLK_OUT_1                   W_OFFSET + 31
#define OSC_CLK_OUT_2                   W_OFFSET + 32
#define OSC_CLK_OUT_3                   W_OFFSET + 33
#define OSC_CLK_OUT_4                   W_OFFSET + 34
#define PMU_AC_PRESENT                  W_OFFSET + 35
#define PMU_BATLOW_B                    W_OFFSET + 36
#define PMU_PLTRST_B                    W_OFFSET + 37
#define PMU_PWRBTN_B                    W_OFFSET + 38
#define PMU_RESETBUTTON_B               W_OFFSET + 39
#define PMU_SLP_S0_B                    W_OFFSET + 40
#define PMU_SLP_S3_B                    W_OFFSET + 41
#define PMU_SLP_S4_B                    W_OFFSET + 42
#define PMU_SUSCLK                      W_OFFSET + 43
#define PMU_WAKE_B                      W_OFFSET + 44
#define SUS_STAT_B                      W_OFFSET + 45
#define SUSPWRDNACK                     W_OFFSET + 46

/* Southwest community pads */
#define SW_OFFSET                       (W_OFFSET + 47)
#define GPIO_205                        SW_OFFSET + 0
#define GPIO_206                        SW_OFFSET + 1
#define GPIO_207                        SW_OFFSET + 2
#define GPIO_208                        SW_OFFSET + 3
#define GPIO_156                        SW_OFFSET + 4
#define GPIO_157                        SW_OFFSET + 5
#define GPIO_158                        SW_OFFSET + 6
#define GPIO_159                        SW_OFFSET + 7
#define GPIO_160                        SW_OFFSET + 8
#define GPIO_161                        SW_OFFSET + 9
#define GPIO_162                        SW_OFFSET + 10
#define GPIO_163                        SW_OFFSET + 11
#define GPIO_164                        SW_OFFSET + 12
#define GPIO_165                        SW_OFFSET + 13
#define GPIO_166                        SW_OFFSET + 14
#define GPIO_167                        SW_OFFSET + 15
#define GPIO_168                        SW_OFFSET + 16
#define GPIO_169                        SW_OFFSET + 17
#define GPIO_170                        SW_OFFSET + 18
#define GPIO_171                        SW_OFFSET + 19
#define GPIO_172                        SW_OFFSET + 20
#define GPIO_179                        SW_OFFSET + 21
#define GPIO_173                        SW_OFFSET + 22
#define GPIO_174                        SW_OFFSET + 23
#define GPIO_175                        SW_OFFSET + 24
#define GPIO_176                        SW_OFFSET + 25
#define GPIO_177                        SW_OFFSET + 26
#define GPIO_178                        SW_OFFSET + 27
#define GPIO_186                        SW_OFFSET + 28
#define GPIO_182                        SW_OFFSET + 29
#define GPIO_183                        SW_OFFSET + 30
#define SMB_ALERTB                      SW_OFFSET + 31
#define SMB_CLK                         SW_OFFSET + 32
#define SMB_DATA                        SW_OFFSET + 33
#define LPC_ILB_SERIRQ                  SW_OFFSET + 34
#define LPC_CLKOUT0                     SW_OFFSET + 35
#define LPC_CLKOUT1                     SW_OFFSET + 36
#define LPC_AD0                         SW_OFFSET + 37
#define LPC_AD1                         SW_OFFSET + 38
#define LPC_AD2                         SW_OFFSET + 39
#define LPC_AD3                         SW_OFFSET + 40
#define LPC_CLKRUNB                     SW_OFFSET + 41
#define LPC_FRAMEB                      SW_OFFSET + 42

#define PAD_CFG_DW_OFFSET       0x500
#define PAD_CFG_DW_OFFSET_MUL   8

#endif /* __DRIVERS_GPIO_APL_DEFS_H__ */
