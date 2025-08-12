/* pinmux register offset */

#include <limits.h>

#define BITS(bity, bitx) \
    (((unsigned int)(0xFFFFFFFF) >> (32 - ((bity) - (bitx) + 1))) << (bitx))

// #define CAN_MASK(bitx) (1 << x)

#define RW  0
#define R   1
#define W   3

#define CAN0_BASE	0x29240000
#define CAN1_BASE	0x29250000



#define BIT(x) (unsigned int)(1 << x)


// REG_CLASS 
#define CANT_READ   BIT(0)
#define CONTROL     BIT(1)
#define ACCEP_MASK  BIT(2)
#define BITRATE     BIT(3)
#define ARB         BIT(4)
#define FIFO_CTRL   BIT(5)
#define ERR         BIT(6)
#define IRQ         BIT(7)
#define STATUS      BIT(8)

struct can_func {
    const char name[32];
    unsigned int mask;
    unsigned int start;
    unsigned int end;
    unsigned int reset;
};

struct can_reg {
    const char name[32];
    unsigned int reg_class;
    unsigned int index;
    const struct can_func funcs[8];
    unsigned int rw;
    unsigned int reg_flag_list;
    
};

struct can_reg can_reg_map[] = {
    {"CONTROL",CONTROL ,0 , {
        {"CTRL_RESET",       BIT(0), 0, 0},
        {"CTRL_CLK_OFF",     BIT(1), 1, 1},
        {"CTRL_FRAME_MODE",  BIT(2), 2, 2},
        {"CTRL_MODE",        BITS(4,3), 4, 3},
        {"RESERVED",         BITS(7,5), 7, 5}
    }},

    {"EXT CONTROL",CONTROL , 1, {
        {"EXT_CTRL_LISTEN",      BIT(0), 0, 0},
        {"EXT_CTRL_SELF_TEST",   BIT(1), 1, 1},
        {"EXT_CTRL_ACC_FILTER",  BIT(2), 2, 2},
        {"EXT_CTRL_REC_INCR",    BIT(3), 3, 3},
        {"EXT_CTRL_REMOTE_EN",   BIT(4), 4, 4},
        {"EXT_ID_RX",            BIT(5), 5, 5},
        {"RESERVED",             BITS(7,6), 7, 6}
    }},

    {"FD CONTROL", CONTROL, 2, {
        {"FD_BRS", BIT(0), 0, 0, 0x1},
        {"FD_TX",  BIT(1), 1, 1}
    }},

    {"COMMAND", CONTROL, 3, {
        {"CMD_REQ",             BIT(0), 0, 0},
        {"CMD_EXT_ID",          BIT(1), 1, 1},
        {"CMD_REMOTE_RDY",      BIT(2), 2, 2},
        {"CMD_ABORT",           BIT(3), 3, 3},
        {"CMD_REL_RX_BUF",      BIT(4), 4, 4},
        {"CMD_CLEAR_OVERRUN",   BIT(5), 5, 5},
        {"CMD_REQ_OVERLOAD",    BIT(6), 6, 6},
        {"CMD_REQ_SLEEP",       BIT(7), 7, 7}
    }},

    {"DLC REMOTE FRAME", CONTROL, 4, {
        {"REMOTE_DLC",      BITS(3,0), 3, 0},
        {"REMOTE_EXT_ID",   BIT(4), 4, 4},
        {"REMOTE_RSP_VALID", BIT(5), 5, 5}
    }},

    {"STATUS", STATUS, 5, {
        {"STATUS_RX_BUF",   BIT(0), 0, 0},
        {"STATUS_OVERRUN",  BIT(1), 1, 1},
        {"STATUS_TX_BUF",   BIT(2), 2, 2},
        {"STATUS_TX_DONE",  BIT(3), 3, 3},
        {"STATUS_RX",       BIT(4), 4, 4},
        {"STATUS_TX",       BIT(5), 5, 5},
        {"STATUS_ERR",      BIT(6), 6, 6},
        {"STATUS_BUS_OFF",  BIT(7), 7, 7}
    }, R},

    {"ACCEPTANCE CODE0", ACCEP_MASK, 6, { {"ACCEPTANCE_CODE0", BITS(7,0), 7, 0} } },
    {"ACCEPTANCE CODE1", ACCEP_MASK, 7, { {"ACCEPTANCE_CODE1", BITS(7,0), 7, 0} } },
    {"ACCEPTANCE CODE2", ACCEP_MASK, 8, { {"ACCEPTANCE_CODE2", BITS(7,0), 7, 0} } },
    {"ACCEPTANCE CODE3", ACCEP_MASK, 9, { {"ACCEPTANCE_CODE3", BITS(7,0), 7, 0} } },

    {"ACCEPTANCE MASK0", ACCEP_MASK, 10, { {"ACCEPTANCE_MASK0", BITS(7,0), 7, 0} } },
    {"ACCEPTANCE MASK1", ACCEP_MASK, 11, { {"ACCEPTANCE_MASK1", BITS(7,0), 7, 0} } },
    {"ACCEPTANCE MASK2", ACCEP_MASK, 12, { {"ACCEPTANCE_MASK2", BITS(7,0), 7, 0} } },
    {"ACCEPTANCE MASK3", ACCEP_MASK, 13, { {"ACCEPTANCE_MASK3", BITS(7,0), 7, 0} } },

    {"NOR CLK DIVIDER", BITRATE, 14, {
        {"NOR_CLK_DIVIDER8", BITS(7,0), 7, 0}
    }},
    {"NOR BUS TIMING0", BITRATE, 15, {
        {"BAUD_PRESCALER", BITS(5,0), 5, 0},
        {"SYNC_JUMP_WIDTH", BITS(7,6), 7, 6}
    }},
    {"NOR BUS TIMING1", BITRATE, 16, {
        {"TIME_SEGMENT1", BITS(3,0), 3, 0},
        {"TIME_SEGMENT2", BITS(6,4), 6, 4}
    }},

    {"FD CLK DIVIDER", BITRATE, 17, {
        {"FD_CLK_DIVIDER8", BITS(7,0), 7, 0}
    }},
    {"FD BUS TIMING0", BITRATE, 18, {
        {"FD_BAUD_PRESCALER", BITS(5,0), 5, 0},
        {"FD_SYNC_JUMP_WIDTH", BITS(7,6), 7, 6}
    }},
    {"FD BUS TIMING1", BITRATE, 19, {
        {"FD_TIME_SEGMENT1", BITS(3,0), 3, 0},
        {"FD_TIME_SEGMENT2", BITS(6,4), 6, 4},
        {"FD_TIME_TRIPLE_SAMPLE", BIT(7), 7, 7}
    }},

    {"ARB ID0", ARB, 20, { {"ARB_ID0", BITS(7,0), 7, 0} }},
    {"ARB ID1", ARB, 21, { {"ARB_ID1", BITS(7,0), 7, 0} }},
    {"ARB ID2", ARB, 22, { {"ARB_ID2", BITS(7,0), 7, 0} }},
    {"ARB ID3", ARB, 23, { {"ARB_ID3", BITS(7,0), 7, 0} }},
    {"ARB ID4", ARB, 24, { {"ARB_ID4", BITS(7,0), 7, 0} }},

    {"RSP_ARB ID0", ARB, 25, { {"RSP_ARB_ID0", BITS(7,0), 7, 0} }},
    {"RSP_ARB ID1", ARB, 26, { {"RSP_ARB_ID1", BITS(7,0), 7, 0} }},
    {"RSP_ARB ID2", ARB, 27, { {"RSP_ARB_ID2", BITS(7,0), 7, 0} }},
    {"RSP_ARB ID3", ARB, 28, { {"RSP_ARB_ID3", BITS(7,0), 7, 0} }},

    {"TX DATA FIFO",CANT_READ , 29, { {"TX_DATA_FIFO", BITS(7,0), 7, 0} }, W, 1},
    {"RX DATA FIFO",CANT_READ , 30, { {"RX_DATA_FIFO", BITS(7,0), 7, 0} }, R, 1},
    {"RX LEN FIFO",CANT_READ ,  31, { {"RX_LEN_FIFO", BITS(7,0), 7, 0} },  R, 1},
    {"TX RSP FIFO",CANT_READ ,  32, { {"TX_RSP_FIFO", BITS(7,0), 7, 0} },  W, 1},

// ERROR Registers
    {"ARB LOST CAPTURE",ERR , 33, {
        {"ARB_LOST_CAPTURE5", BITS(4,0), 4, 0}
    }},

    {"ERR CODE CAPTURE",ERR, 34, {
        {"ERR_CODE_SEG", BITS(4,0), 4, 0},
        {"ERR_CODE_DIR", BIT(5), 5, 5},
        {"ERR_CODE_TYPE", BITS(7,6), 7, 6}
    }},

    {"ERR WARN LIMIT",ERR, 35, { {"ERR_WARN_LIMIT", BITS(7,0), 7, 0} }},
    {"RX ERR COUNTER",ERR, 36, { {"RX_ERR_COUNTER", BITS(7,0), 7, 0} }, R},
    {"TX ERR COUNTER",ERR, 37, { {"TX_ERR_COUNTER", BITS(7,0), 7, 0} }, R},
    // Backdoor load the transmit/receiver error counter
    {"ERR COUNTER LOAD",ERR, 38, {
        {"TX_LOAD_ERR",ERR, BIT(0), 0, 0},
        {"RX_LOAD_ERR",ERR, BIT(1), 1, 1}
    }},
    {"ERR DATA",ERR, 39, { {"ERR_DATA8", BITS(7,0), 7, 0} }},

// FIFO Related Registers
    {"FIFO FLUSH",CANT_READ , 40, {
        {"TX_DATA_FIFO_FLUSH", BIT(0), 0, 0},
        {"RX_DATA_FIFO_FLUSH", BIT(1), 1, 1},
        {"RX_LEN_FIFO_FLUSH", BIT(2), 2, 2},
        {"TX_RSP_FIFO_FLUSH", BIT(3), 3, 3}
    } , W},

    {"TX DATA FIFO Edge",CANT_READ , 41, { {"TX_DATA_FIFO Edge", BITS(7,0), 7, 0} }, W},
    {"TX RSP FIFO Edge",CANT_READ ,  42, { {"TX_RSP_FIFO Edge", BITS(7,0), 7, 0} },  W},
    {"RX DATA FIFO Edge",CANT_READ , 43, { {"RX_DATA_FIFO Edge", BITS(7,0), 7, 0} }, W},
    {"RX LEN FIFO Edge",CANT_READ ,  44, { {"RX_LEN_FIFO Edge", BITS(7,0), 7, 0} },  W},

    {"FIFO STATUS",FIFO_CTRL |STATUS ,45, {
        {"TX_DFIFO_FULL", BIT(0), 0, 0},
        {"TX_DFIFO_EMPTY", BIT(1), 1, 1},
        {"RX_DFIFO_FULL", BIT(2), 2, 2},
        {"RX_DFIFO_EMPTY", BIT(3), 3, 3},
        {"RX_LFIFO_FULL", BIT(4), 4, 4},
        {"RX_LFIFO_EMPTY", BIT(5), 5, 5},
        {"TX_RFIFO_FULL", BIT(6), 6, 6},
        {"TX_RFIFO_EMPTY", BIT(7), 7, 7}
    }, R},

    {"TX DATA FF_DEPTH",FIFO_CTRL , 46, { {"TX_DATA_FIFO_DEPTH", BITS(7,0), 7, 0} }, R},
    {"TX DATA FF_AVAIL",FIFO_CTRL , 47, { {"TX_DATA_FIFO_AVAIL", BITS(7,0), 7, 0} }, R},
    {"TX RSP FF_DEPTH",FIFO_CTRL ,  48, { {"TX_RSP_FF_DEPTH", BITS(7,0), 7, 0} },  R},
    {"TX RSP FF_AVAIL",FIFO_CTRL ,  49, { {"TX_RSP_FF_AVAIL", BITS(7,0), 7, 0} },  R},
    {"RX LEN FF_DEPTH",FIFO_CTRL ,  50, { {"RX_LEN_FF_DEPTH", BITS(7,0), 7, 0} },  R},
    {"RX LEN FF_AVAIL",FIFO_CTRL ,  51, { {"RX_LEN_FF_AVAIL", BITS(7,0), 7, 0} },  R},
    {"TX RSP FF_DEPTH",FIFO_CTRL ,  52, { {"TX_RSP_FF_DEPTH", BITS(7,0), 7, 0} },  R},
    {"TX RSP FF_AVAIL",FIFO_CTRL ,  53, { {"TX_RSP_FF_AVAIL", BITS(7,0), 7, 0} },  R},
//IRQ Registers
    {"IRQ ENABLE0",IRQ ,54 , {
        {"IRQ_INFO_EMPTY", BIT(0), 0, 0},
        {"IRQ_TRANSMIT_BUFFER_STATUS", BIT(1), 1, 1},
        {"IRQ_ERROR_STATUS", BIT(2), 2, 2},
        {"IRQ_DATA_OVER_RUN", BIT(3), 3, 3},
        {"IRQ_REMOTE_FRAME", BIT(4), 4, 4},
        {"IRQ_NODE_ERROR_PASSIVE", BIT(5), 5, 5},
        {"IRQ_ARBITRATION_LOST", BIT(6), 6, 6},
        {"IRQ_BUS_ERR", BIT(7), 7, 7}
    }},
    {"IRQ STATUS0", IRQ, 55 , {
        {"IRQ_INFO_EMPTY", BIT(0), 0, 0},
        {"IRQ_TRANSMIT_BUFFER_STATUS", BIT(1), 1, 1},
        {"IRQ_ERROR_STATUS", BIT(2), 2, 2},
        {"IRQ_DATA_OVER_RUN", BIT(3), 3, 3},
        {"IRQ_REMOTE_FRAME", BIT(4), 4, 4},
        {"IRQ_NODE_ERROR_PASSIVE", BIT(5), 5, 5},
        {"IRQ_ARBITRATION_LOST", BIT(6), 6, 6},
        {"IRQ_BUS_ERR", BIT(7), 7, 7}
    }},
    {"IRQ ENABLE1", IRQ, 56 , {
        {"IRQ_TX_DONE", BIT(0), 0, 0},
        {"IRQ_RX_DATA_FRAME", BIT(1), 1, 1}
    }},
    {"IRQ STATUS1", IRQ, 57 , {
        {"IRQ_TX_DONE", BIT(0), 0, 0},
        {"IRQ_RX_DATA_FRAME", BIT(1), 1, 1}
    }},
    {"IRQ ENABLE2", IRQ, 58 , {
        {"IRQ_TX_DATA_FIFO_TT", BIT(0), 0, 0},
        {"IRQ_TX_DATA_FIFO_UR", BIT(1), 1, 1},
        {"IRQ_TX_DATA_FIFO_OR", BIT(2), 2, 2},
        {"IRQ_TX_DATA_FIFO_FULL", BIT(3), 3, 3},
        {"IRQ_TX_DATA_FIFO_EMPTY", BIT(4), 4, 4}
    }},
    {"IRQ STATUS2", IRQ, 59 , {
        {"IRQ_TX_DATA_FIFO_TT", BIT(0), 0, 0},
        {"IRQ_TX_DATA_FIFO_UR", BIT(1), 1, 1},
        {"IRQ_TX_DATA_FIFO_OR", BIT(2), 2, 2},
        {"IRQ_TX_DATA_FIFO_FULL", BIT(3), 3, 3},
        {"IRQ_TX_DATA_FIFO_EMPTY", BIT(4), 4, 4}
    }},
    {"IRQ ENABLE3", IRQ, 60 , {
        {"IRQ_TX_RSP_FIFO_TT", BIT(0), 0, 0},
        {"IRQ_TX_RSP_FIFO_UR", BIT(1), 1, 1},
        {"IRQ_TX_RSP_FIFO_OR", BIT(2), 2, 2},
        {"IRQ_TX_RSP_FIFO_FULL", BIT(3), 3, 3},
        {"IRQ_TX_RSP_FIFO_EMPTY", BIT(4), 4, 4}
    }},
    {"IRQ STATUS3", IRQ, 61 , {
        {"IRQ_TX_RSP_FIFO_TT", BIT(0), 0, 0},
        {"IRQ_TX_RSP_FIFO_UR", BIT(1), 1, 1},
        {"IRQ_TX_RSP_FIFO_OR", BIT(2), 2, 2},
        {"IRQ_TX_RSP_FIFO_FULL", BIT(3), 3, 3},
        {"IRQ_TX_RSP_FIFO_EMPTY", BIT(4), 4, 4}
    }},
    {"IRQ ENABLE4", IRQ, 62, {
        {"IRQ_RX_DATA_FIFO_TT", BIT(0), 0, 0},
        {"IRQ_RX_DATA_FIFO_UR", BIT(1), 1, 1},
        {"IRQ_RX_DATA_FIFO_OR", BIT(2), 2, 2},
        {"IRQ_RX_DATA_FIFO_FULL", BIT(3), 3, 3},
        {"IRQ_RX_DATA_FIFO_EMPTY", BIT(4), 4, 4}
    }},
    {"IRQ STATUS4",IRQ ,63 , {
        {"IRQ_RX_DATA_FIFO_TT", BIT(0), 0, 0},
        {"IRQ_RX_DATA_FIFO_UR", BIT(1), 1, 1},
        {"IRQ_RX_DATA_FIFO_OR", BIT(2), 2, 2},
        {"IRQ_RX_DATA_FIFO_FULL", BIT(3), 3, 3},
        {"IRQ_RX_DATA_FIFO_EMPTY", BIT(4), 4, 4}
    }},
    {"IRQ ENABLE5", IRQ, 64, {
        {"IRQ_RX_LEN_FIFO_TT", BIT(0), 0, 0},
        {"IRQ_RX_LEN_FIFO_UR", BIT(1), 1, 1},
        {"IRQ_RX_LEN_FIFO_OR", BIT(2), 2, 2},
        {"IRQ_RX_LEN_FIFO_FULL", BIT(3), 3, 3},
        {"IRQ_RX_LEN_FIFO_EMPTY", BIT(4), 4, 4}
    }},
    {"IRQ STATUS5", IRQ, 65, {
        {"IRQ_RX_LEN_FIFO_TT", BIT(0), 0, 0},
        {"IRQ_RX_LEN_FIFO_UR", BIT(1), 1, 1},
        {"IRQ_RX_LEN_FIFO_OR", BIT(2), 2, 2},
        {"IRQ_RX_LEN_FIFO_FULL", BIT(3), 3, 3},
        {"IRQ_RX_LEN_FIFO_EMPTY", BIT(4), 4, 4}
    }},
    {"SOC TIMEOUT", IRQ, 66, {
        {"SOC_TIMEOUT", BITS(7,0), 7, 0, 0XF}
    }},

};
