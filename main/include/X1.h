/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            X1.h
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Header for the Sharp X1 to PS/2 interface logic.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Mar 2022 - Initial write.
//            v1.01 May 2022 - Initial release version.
//            v1.02 Jun 2022 - Updates to reflect changes realised in other modules due to addition of
//                             bluetooth and suspend logic due to NVS issues using both cores.
//            v1.03 Jun 2022 - Further updates adding in keymaps for UK BT and Japan OADG109.
//
// Notes:           See Makefile to enable/disable conditional components
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// This source file is free software: you can redistribute it and#or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This source file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef X1_H
#define X1_H

// Include the specification class.
#include "KeyInterface.h"
#include "NVS.h"
#include "LED.h"
#include "HID.h"
#include <vector>
#include <map>

// NB: Macros definitions put inside class for clarity, they are still global scope.

// Encapsulate the Sharp X1 interface.
class X1 : public KeyInterface  {
    // Macros.
    //
    #define NUMELEM(a)                      (sizeof(a)/sizeof(a[0]))
    
    // Constants.
    #define X1IF_VERSION                    1.03
    #define X1IF_KEYMAP_FILE                "X1_KeyMap.BIN"
    #define MAX_X1_XMIT_KEY_BUF             16
    #define PS2TBL_X1_MAXROWS               349
    
    // X1 Key control bit mask.
    #define X1_CTRL_TENKEY                  ((unsigned char) (1 << 7))
    #define X1_CTRL_PRESS                   ((unsigned char) (1 << 6))
    #define X1_CTRL_REPEAT                  ((unsigned char) (1 << 5))
    #define X1_CTRL_GRAPH                   ((unsigned char) (1 << 4))
    #define X1_CTRL_CAPS                    ((unsigned char) (1 << 3))
    #define X1_CTRL_KANA                    ((unsigned char) (1 << 2))
    #define X1_CTRL_SHIFT                   ((unsigned char) (1 << 1))
    #define X1_CTRL_CTRL                    ((unsigned char) (1 << 0))
    
    // X1 Special Key definitions.
    #define X1KEY_UP                        0x1E                           // ↑
    #define X1KEY_DOWN                      0x1F                           // ↓
    #define X1KEY_LEFT                      0x1D                           // ←
    #define X1KEY_RIGHT                     0x1C                           // → →
    #define X1KEY_INS                       0x12                           // INS
    #define X1KEY_DEL                       0x08                           // DEL
    #define X1KEY_CLR                       0x0C                           // CLR
    #define X1KEY_HOME                      0x0B                           // HOME
    
    // PS2 Flag definitions.
    #define PS2CTRL_NONE                    0x00                           // No keys active = 0
    #define PS2CTRL_SHIFT                   0x01                           // Shfit Key active = 1
    #define PS2CTRL_CTRL                    0x02                           // Ctrl Key active = 1
    #define PS2CTRL_CAPS                    0x04                           // CAPS active = 1
    #define PS2CTRL_KANA                    0x08                           // KANA active = 1
    #define PS2CTRL_GRAPH                   0x10                           // GRAPH active = 1
    #define PS2CTRL_GUI                     0x20                           // GUI Key active = 1
    #define PS2CTRL_FUNC                    0x40                           // Special Function Keys active = 1
    #define PS2CTRL_BREAK                   0x80                           // BREAK Key active = 1
    #define PS2CTRL_EXACT                   0x80                           // EXACT Match active = 1
    
    // The initial mapping is made inside the PS2KeyAdvanced class from Scan Code Set 2 to ASCII
    // for a selected keyboard. Special functions are detected and combined inside this module
    // before mapping with the table below to extract the X1 key code and control data.
    // ie. PS/2 Scan Code -> ASCII + Flags -> X1 Key Code + Ctrl Data

    // Keyboard mapping table column names.
    #define PS2TBL_PS2KEYCODE_NAME          "PS/2 KeyCode"
    #define PS2TBL_PS2CTRL_NAME             "PS/2 Control Key"
    #define PS2TBL_KEYBOARDMODEL_NAME       "For Keyboard"
    #define PS2TBL_MACHINE_NAME             "For Host Model"
    #define PS2TBL_X1MODE_NAME              "X1 Mode"
    #define PS2TBL_X1KEYCODE_NAME           "X1 KeyCode 1"
    #define PS2TBL_X1KEYCODE_BYTE2_NAME     "X1 KeyCode 2"
    #define PS2TBL_X1_CTRL_NAME             "X1 Control Key"

    // Keyboard mapping table column types.
    #define PS2TBL_PS2KEYCODE_TYPE          "hex"
    #define PS2TBL_PS2CTRL_TYPE             "custom_cbp_ps2ctrl"
    #define PS2TBL_KEYBOARDMODEL_TYPE       "custom_cbp_keybmodel"
    #define PS2TBL_MACHINE_TYPE             "custom_cbp_machine"
    #define PS2TBL_X1MODE_TYPE              "custom_cbp_x1mode"
    #define PS2TBL_X1KEYCODE_TYPE           "hex"
    #define PS2TBL_X1KEYCODE_BYTE2_TYPE     "hex"
    #define PS2TBL_X1CTRL_TYPE              "custom_cbn_x1ctrl"

    // Keyboard mapping table select list for PS2CTRL.
    #define PS2TBL_PS2CTRL_SEL_NONE         "NONE"
    #define PS2TBL_PS2CTRL_SEL_SHIFT        "SHIFT"
    #define PS2TBL_PS2CTRL_SEL_CTRL         "CTRL"
    #define PS2TBL_PS2CTRL_SEL_CAPS         "CAPS"
    #define PS2TBL_PS2CTRL_SEL_KANA         "KANA"
    #define PS2TBL_PS2CTRL_SEL_GRAPH        "GRAPH"
    #define PS2TBL_PS2CTRL_SEL_GUI          "GUI"
    #define PS2TBL_PS2CTRL_SEL_FUNC         "FUNC"
    #define PS2TBL_PS2CTRL_SEL_EXACT        "EXACT"

    // Keyboard mapping table select list for Model of keyboard.
    #define KEYMAP_SEL_STANDARD             "ALL"
    #define KEYMAP_SEL_UK_WYSE_KB3926       "UK_WYSE_KB3926"
    #define KEYMAP_SEL_JAPAN_OADG109        "JAPAN_OADG109"
    #define KEYMAP_SEL_JAPAN_SANWA_SKBL1    "JAPAN_SANWA_SKBL1"
    #define KEYMAP_SEL_NOT_ASSIGNED_4       "KEYBOARD_4"
    #define KEYMAP_SEL_NOT_ASSIGNED_5       "KEYBOARD_5"
    #define KEYMAP_SEL_NOT_ASSIGNED_6       "KEYBOARD_6"
    #define KEYMAP_SEL_UK_PERIBOARD_810     "UK_PERIBOARD_810"
    #define KEYMAP_SEL_UK_OMOTON_K8508      "UK_OMOTON_K8508"
   
    // Keyboard mapping table select list for keyboard mode.
    #define X1_SEL_MODE_A                   "Mode_A"
    #define X1_SEL_MODE_B                   "Mode_B"

    // Keyboard mapping table select list for target machine.
    #define X1_SEL_ALL                      "ALL"
    #define X1_SEL_ORIG                     "ORIGINAL"
    #define X1_SEL_TURBO                    "TURBO"
    #define X1_SEL_TURBOZ                   "TURBOZ"

    // Keyboard mapping table select list for X1 Control codes.
    #define X1_CTRL_SEL_TENKEY              "TENKEY"
    #define X1_CTRL_SEL_PRESS               "PRESS"
    #define X1_CTRL_SEL_REPEAT              "REPEAT"
    #define X1_CTRL_SEL_GRAPH               "GRAPH"
    #define X1_CTRL_SEL_CAPS                "CAPS"
    #define X1_CTRL_SEL_KANA                "KANA"
    #define X1_CTRL_SEL_SHIFT               "SHIFT"
    #define X1_CTRL_SEL_CTRL                "CTRL"

    // The Sharp X1 Series was released over a number of years and each iteration added changes/updates. In order to cater for differences, it is possible to assign a key mapping
    // to a specific machine type(s) or all of the series by adding the flags below into the mapping table.
    #define X1_ALL                          0xFF
    #define X1_ORIG                         0x01
    #define X1_TURBO                        0x02
    #define X1_TURBOZ                       0x04
    
    // The X1 Turbo onwards had a mode switch on the keyboard, Mode A was normal use, Mode B was for games, speeding up the key press by shortening the timing and setting common game keys pressed in a 24bit bit map.
    // The mapping table caters for both, OR'ing data in Mode B so that multiple key presses are sent across as a bit map.
    #define X1_MODE_A                       0x01
    #define X1_MODE_B                       0x02

    // Keyboard models. The base on which this interface was created was a Wyse KB3926 PS/2 Keyboard and this is deemed STANDARD. Other models need to insert difference maps
    // prior to the STANDARD entry along with the keyboard model so that it is processed first thus allowing differing keyboards with different maps.
    #define KEYMAP_STANDARD                 0xFF
    #define KEYMAP_UK_WYSE_KB3926           0x01
    #define KEYMAP_JAPAN_OADG109            0x02
    #define KEYMAP_JAPAN_SANWA_SKBL1        0x04
    #define KEYMAP_NOT_ASSIGNED_4           0x08
    #define KEYMAP_NOT_ASSIGNED_5           0x10
    #define KEYMAP_NOT_ASSIGNED_6           0x20
    #define KEYMAP_UK_PERIBOARD_810         0x40
    #define KEYMAP_UK_OMOTON_K8508          0x80

    public:
        // Prototypes.
                                        X1(void);
                                        X1(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID, const char *fsPath);
                                        X1(NVS *hdlNVS, HID *hdlHID, const char *fsPath);
                                       ~X1(void);
        bool                            createKeyMapFile(std::fstream &outFile);
        bool                            storeDataToKeyMapFile(std::fstream &outFile, char *data, int size);
        bool                            storeDataToKeyMapFile(std::fstream & outFile, std::vector<uint32_t>& dataArray);
        bool                            closeAndCommitKeyMapFile(std::fstream &outFile, bool cleanupOnly);
        std::string                     getKeyMapFileName(void) { return(X1IF_KEYMAP_FILE); };
        void                            getKeyMapHeaders(std::vector<std::string>& headerList);
        void                            getKeyMapTypes(std::vector<std::string>& typeList);
        bool                            getKeyMapSelectList(std::vector<std::pair<std::string, int>>& selectList, std::string option);        
        bool                            getKeyMapData(std::vector<uint32_t>& dataArray, int *row, bool start);

        // Method to return the class version number.
        float version(void)
        {
            return(X1IF_VERSION);
        }

    protected:

    private:
        // Prototypes.
        void                            pushKeyToQueue(bool keybMode, uint32_t key);
        IRAM_ATTR static void           x1Interface( void * pvParameters );
        IRAM_ATTR static void           hidInterface( void * pvParameters );
                  void                  selectOption(uint8_t optionCode);
                  uint32_t              mapKey(uint16_t scanCode);
        bool                            loadKeyMap();
        bool                            saveKeyMap(void);
        void                            init(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID);
        void                            init(NVS *hdlNVS, HID *hdlHID);

//        // Overload the base yield method to include suspension of the PS/2 Keyboard interface. This interface uses interrupts which are not mutex protected and clash with the
//        // WiFi API methods.
//        inline void yield(uint32_t delay)
//        {
//            // If suspended, go into a permanent loop until the suspend flag is reset.
//            if(this->suspend)
//            {
//                // Suspend the keyboard interface.
//                Keyboard->suspend(true);
//
//                // Use the base method logic.
//                KeyInterface::yield(delay);
//                
//                // Release the keyboard interface.
//                Keyboard->suspend(false);
//            } else
//            // Otherwise just delay by the required amount for timing and to give other threads a time slice.
//            {
//                KeyInterface::yield(delay);
//            }
//            return;
//        }

        // Structure to encapsulate a single key map from PS/2 to X1.
        typedef struct {
            uint8_t                     ps2KeyCode;
            uint8_t                     ps2Ctrl;
            uint8_t                     keyboardModel;
            uint8_t                     machine;
            uint8_t                     x1Mode;
            uint8_t                     x1Key;
            uint8_t                     x1Key2;
            uint8_t                     x1Ctrl;
        } t_keyMapEntry;

        // Structure to encapsulate the entire static keyboard mapping table.
        typedef struct {
            t_keyMapEntry               kme[PS2TBL_X1_MAXROWS];
        } t_keyMap;

        // Structure to maintain the X1 interface configuration data. This data is persisted through powercycles as needed.
        typedef struct {
            struct {
                uint8_t                 activeKeyboardMap;      // Model of keyboard a keymap entry is applicable to.
                uint8_t                 activeMachineModel;     // Machine model a keymap entry is applicable to.
            } params;
        } t_x1Config;
       
        // Configuration data.
        t_x1Config                      x1Config;

        // Structure to manage the control signals signifying the state of the X1 keyboard.
        typedef struct {
            bool                        optionSelect;           // Flag to indicate a user requested keyboard configuration option is being selected.
            bool                        modeB;                  // Mode B (Game mode) flag. If set, Mode B active, clear, Mode A active (normal keyboard).
            uint8_t                     keyCtrl;                // Keyboard state flag control.

            std::string                 fsPath;                 // Path on the underlying filesystem where storage is mounted and accessible.
            t_keyMapEntry              *kme;                    // Pointer to an array in memory to contain PS2 to X1 mapping values.
            int                         kmeRows;                // Number of rows in the kme table.
            std::string                 keyMapFileName;         // Name of file where extension or replacement key map entries are stored.
            bool                        persistConfig;          // Flag to request saving of the config into NVS storage.
        } t_x1Control;

        // Transmit buffer queue item.
        typedef struct {
            uint32_t                    keyCode;  // 32bit because normal mode A is 16bit, game mode B is 24bit
            bool                        modeB;    // True if in game mode B.
        } t_xmitQueueMessage;

        // Thread handles - one per function, ie. HID interface and host target interface.
        TaskHandle_t                    TaskHostIF = NULL;
        TaskHandle_t                    TaskHIDIF  = NULL;

        // Control structure to control interaction and mapping of keys for the host.
        t_x1Control                     x1Control;
       
        // Spin lock mutex to hold a coresied to an uninterruptable method. This only works on dual core ESP32's.
        portMUX_TYPE                    x1Mutex;

        // Lookup table to match PS/2 codes to X1 Key and Control Data.
        //
        // Given that the X1 had many variants, with potential differences between them, the mapping table allows for ALL or variant specific entries, the first entry matching is selected.
        //
     
        //
        // This mapping is for the UK Wyse KB-3926 PS/2 keyboard
        //
        t_keyMap                        PS2toX1 = {
        {
            // HELP
            // COPY
            //                                                                                                                                ModeB Byte1   ModeB Byte2  ModeB Byte3
            //PS2 Code           PS2 Ctrl (Flags to Match)                   Keyboard Model               Machine               X1 Keyb Mode  X1 Data                    X1 Ctrl (Flags to Set).
            { PS2_KEY_F1,        PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'v',          0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // SHIFT+F1
            { PS2_KEY_F2,        PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'w',          0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // SHIFT+F2
            { PS2_KEY_F3,        PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'x',          0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // SHIFT+F3
            { PS2_KEY_F4,        PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'y',          0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // SHIFT+F4
            { PS2_KEY_F5,        PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'z',          0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // SHIFT+F5
            { PS2_KEY_F1,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'q',          0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // F1
            { PS2_KEY_F2,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'r',          0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // F2
            { PS2_KEY_F3,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    's',          0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // F3
            { PS2_KEY_F4,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    't',          0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // F4
            { PS2_KEY_F5,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'u',          0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // F5
            { PS2_KEY_F6,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xEC,         0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // F6
            { PS2_KEY_F7,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xEB,         0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // F7
            { PS2_KEY_F8,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE2,         0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // F8
            { PS2_KEY_F9,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE1,         0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // F9
            { PS2_KEY_F10,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // XFER
            { PS2_KEY_F11,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xFE,         0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // HELP
            { PS2_KEY_F12,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // COPY
            { PS2_KEY_TAB,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x09,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // TAB
            // Control keys.
        	{ PS2_KEY_APOS,      PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-@
        	{ PS2_KEY_A,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x01,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-A
        	{ PS2_KEY_B,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x02,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-B
        	{ PS2_KEY_C,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x03,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-C
        	{ PS2_KEY_D,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x04,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-D
        	{ PS2_KEY_E,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x05,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-E
        	{ PS2_KEY_F,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x06,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-F
        	{ PS2_KEY_G,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x07,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-G
        	{ PS2_KEY_H,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x08,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-H
        	{ PS2_KEY_I,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x09,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-I
        	{ PS2_KEY_J,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x0A,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-J
        	{ PS2_KEY_K,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x0B,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-K
        	{ PS2_KEY_L,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x0C,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-L
        	{ PS2_KEY_M,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x0D,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-M
        	{ PS2_KEY_N,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x0E,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-N
        	{ PS2_KEY_O,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x0F,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-O
        	{ PS2_KEY_P,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x10,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-P
        	{ PS2_KEY_Q,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x11,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-Q
        	{ PS2_KEY_R,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x12,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-R
        	{ PS2_KEY_S,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x13,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-S
        	{ PS2_KEY_T,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x14,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-T
        	{ PS2_KEY_U,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x15,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-U
        	{ PS2_KEY_V,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x16,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-V
        	{ PS2_KEY_W,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x17,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-W
        	{ PS2_KEY_X,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x18,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-X
        	{ PS2_KEY_Y,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x19,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-Y
        	{ PS2_KEY_Z,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x1A,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-Z
            { PS2_KEY_OPEN_SQ,   PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x1B,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-[
            { PS2_KEY_BACK,      PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x1C,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-BACKSLASH
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x1D,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-]
        	{ PS2_KEY_6,         PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x1E,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-^
            { PS2_KEY_MINUS,     PS2CTRL_CTRL,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x1F,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CTRL-_
            // Numeric keys.
        	{ PS2_KEY_0,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '0',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // 0
        	{ PS2_KEY_1,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '1',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // 1
        	{ PS2_KEY_2,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '2',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // 2
        	{ PS2_KEY_3,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '3',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // 3
        	{ PS2_KEY_4,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '4',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // 4
        	{ PS2_KEY_5,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '5',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // 5
        	{ PS2_KEY_6,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '6',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // 6
        	{ PS2_KEY_7,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '7',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // 7
        	{ PS2_KEY_8,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '8',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // 8
        	{ PS2_KEY_9,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '9',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // 9
            // Punctuation keys.
        	{ PS2_KEY_0,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    ')',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Close Right Bracket )
        	{ PS2_KEY_1,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '!',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Exclamation
        	{ PS2_KEY_2,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '"',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Double quote.
            { PS2_KEY_3,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x23,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Pound Sign -> Hash
        	{ PS2_KEY_4,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '$',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Dollar
        	{ PS2_KEY_5,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '%',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Percent
        	{ PS2_KEY_6,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '^',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Kappa
        	{ PS2_KEY_7,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '&',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Ampersand
        	{ PS2_KEY_8,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '*',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Star
        	{ PS2_KEY_9,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '(',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Open Left Bracket (
            // ALPHA keys, lower and uppercase.        
            //                                                                                                                                ModeB Byte1   ModeB Byte2  ModeB Byte3
            //PS2 Code           PS2 Ctrl (Flags to Match)                                                Machine               X1 Keyb Mode  X1 Data                    X1 Ctrl (Flags to Set).
        	{ PS2_KEY_A,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'a',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // a
        	{ PS2_KEY_A,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'A',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // A
            { PS2_KEY_B,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'b',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // b
            { PS2_KEY_B,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'B',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // B
            { PS2_KEY_C,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'c',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // c
            { PS2_KEY_C,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'C',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // C
            { PS2_KEY_D,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'd',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // d
            { PS2_KEY_D,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'D',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // D
            { PS2_KEY_E,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'e',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // e
            { PS2_KEY_E,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'E',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // E
            { PS2_KEY_F,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'f',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // f
            { PS2_KEY_F,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'F',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // F
            { PS2_KEY_G,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'g',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // g
            { PS2_KEY_G,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'G',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // G
            { PS2_KEY_H,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'h',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // h
            { PS2_KEY_H,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'H',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // H
            { PS2_KEY_I,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'i',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // i
            { PS2_KEY_I,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'I',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // I
            { PS2_KEY_J,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'j',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // j
            { PS2_KEY_J,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'J',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // J
            { PS2_KEY_K,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'k',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // k
            { PS2_KEY_K,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'K',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // K
            { PS2_KEY_L,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'l',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // l
            { PS2_KEY_L,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'L',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // L
            { PS2_KEY_M,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'm',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // m
            { PS2_KEY_M,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'M',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // M
            { PS2_KEY_N,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'n',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // n
            { PS2_KEY_N,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'N',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // N
            { PS2_KEY_O,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'o',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // o
            { PS2_KEY_O,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'O',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // O
            { PS2_KEY_P,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'p',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // p
            { PS2_KEY_P,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'P',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // P
            { PS2_KEY_Q,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'q',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // q
            { PS2_KEY_Q,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'Q',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Q
            { PS2_KEY_R,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'r',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // r
            { PS2_KEY_R,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'R',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // R
            { PS2_KEY_S,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    's',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // s
            { PS2_KEY_S,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'S',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // S
            { PS2_KEY_T,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    't',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // t
            { PS2_KEY_T,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'T',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // T
            { PS2_KEY_U,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'u',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // u
            { PS2_KEY_U,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'U',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // U
            { PS2_KEY_V,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'v',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // v
            { PS2_KEY_V,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'V',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // V
            { PS2_KEY_W,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'w',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // w
            { PS2_KEY_W,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'W',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // W
            { PS2_KEY_X,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'x',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // x
            { PS2_KEY_X,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'X',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // X
            { PS2_KEY_Y,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'y',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // y
            { PS2_KEY_Y,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'Y',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Y
            { PS2_KEY_Z,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'z',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // z
            { PS2_KEY_Z,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    'Z',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Z
            // Mode B Mappings.
            { PS2_KEY_Q,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b10000000,   0b00000000,  0b00000000,                                                          },   // MODE B - Q
            { PS2_KEY_W,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b01000000,   0b00000000,  0b00000000,                                                          },   // MODE B - W
            { PS2_KEY_E,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00100000,   0b00000000,  0b00000000,                                                          },   // MODE B - E
            { PS2_KEY_A,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00010000,   0b00000000,  0b00000000,                                                          },   // MODE B - A
            { PS2_KEY_D,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00001000,   0b00000000,  0b00000000,                                                          },   // MODE B - D
            { PS2_KEY_Z,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000100,   0b00000000,  0b00000000,                                                          },   // MODE B - Z
            { PS2_KEY_X,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000010,   0b00000000,  0b00000000,                                                          },   // MODE B - X
            { PS2_KEY_C,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000001,   0b00000000,  0b00000000,                                                          },   // MODE B - C
            { PS2_KEY_I,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000000,  0b01000000,                                                          },   // MODE B - I - this is not 100%, the specs arent clear.
            { PS2_KEY_1,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00100000,  0b00000000,                                                          },   // MODE B - 1
            { PS2_KEY_2,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00001000,  0b00000000,                                                          },   // MODE B - 2
            { PS2_KEY_3,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000001,  0b00000000,                                                          },   // MODE B - 3
            { PS2_KEY_4,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b01000000,  0b00000000,                                                          },   // MODE B - 4
            { PS2_KEY_6,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000010,  0b00000000,                                                          },   // MODE B - 6
            { PS2_KEY_7,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b10000000,  0b00000000,                                                          },   // MODE B - 7
            { PS2_KEY_8,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00010000,  0b00000000,                                                          },   // MODE B - 8
            { PS2_KEY_9,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000100,  0b00000000,                                                          },   // MODE B - 9
            { PS2_KEY_ESC,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000000,  0b10000000,                                                          },   // MODE B - ESC
            { PS2_KEY_MINUS,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000000,  0b00100000,                                                          },   // MODE B - MINUS
            { PS2_KEY_EQUAL,     PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000000,  0b00010000,                                                          },   // MODE B - PLUS
            { PS2_KEY_8,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000000,  0b00001000,                                                          },   // MODE B - TIMES
            { PS2_KEY_TAB,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000000,  0b00000100,                                                          },   // MODE B - TAB
            { PS2_KEY_SPACE,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000000,  0b00000010,                                                          },   // MODE B - SPACE
            { PS2_KEY_ENTER,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000000,  0b00000001,                                                          },   // MODE B - RET
            { PS2_KEY_KP1,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00100000,  0b00000000,                                                          },   // MODE B - KeyPad 1
            { PS2_KEY_KP2,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00001000,  0b00000000,                                                          },   // MODE B - KeyPad 2
            { PS2_KEY_KP3,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000001,  0b00000000,                                                          },   // MODE B - KeyPad 3
            { PS2_KEY_KP4,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b01000000,  0b00000000,                                                          },   // MODE B - KeyPad 4
            { PS2_KEY_KP6,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000010,  0b00000000,                                                          },   // MODE B - KeyPad 6
            { PS2_KEY_KP7,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b10000000,  0b00000000,                                                          },   // MODE B - KeyPad 7
            { PS2_KEY_KP8,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00010000,  0b00000000,                                                          },   // MODE B - KeyPad 8
            { PS2_KEY_KP9,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000100,  0b00000000,                                                          },   // MODE B - KeyPad 9
            { PS2_KEY_KP_MINUS,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000000,  0b00100000,                                                          },   // MODE B - KeyPad MINUS
            { PS2_KEY_KP_PLUS,   PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000000,  0b00010000,                                                          },   // MODE B - KeyPad PLUS
            { PS2_KEY_KP_TIMES,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_B,    0b00000000,   0b00000000,  0b00001000,                                                          },   // MODE B - KeyPad TIMES
    
            //                                                                                                                                ModeB Byte1   ModeB Byte2  ModeB Byte3
            //PS2 Code           PS2 Ctrl (Flags to Match)                                                Machine               X1 Keyb Mode  X1 Data                    X1 Ctrl (Flags to Set).
            { PS2_KEY_SPACE,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    ' ',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Space
            { PS2_KEY_COMMA,     PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '<',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Less Than <
            { PS2_KEY_COMMA,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    ',',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Comma ,
            { PS2_KEY_SEMI,      PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    ':',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Colon :
            { PS2_KEY_SEMI,      PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    ';',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Semi-Colon ;
            { PS2_KEY_DOT,       PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '>',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Greater Than >
            { PS2_KEY_DOT,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '.',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Full stop .
            { PS2_KEY_DIV,       PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '?',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Question ?
            { PS2_KEY_DIV,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '/',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Divide /
            { PS2_KEY_MINUS,     PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '_',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Underscore
            { PS2_KEY_MINUS,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '-',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   
            { PS2_KEY_APOS,      PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '@',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // At @
            { PS2_KEY_APOS,      PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '\'',         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Single quote '
            { PS2_KEY_OPEN_SQ,   PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '{',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Open Left Brace {
            { PS2_KEY_OPEN_SQ,   PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '[',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Open Left Square Bracket [
            { PS2_KEY_EQUAL,     PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '+',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Plus +
            { PS2_KEY_EQUAL,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '=',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Equal =
            { PS2_KEY_CAPS,      PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    ' ',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // LOCK
            { PS2_KEY_ENTER,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x0D,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // ENTER/RETURN
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '}',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Close Right Brace }
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    ']',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Close Right Square Bracket ]
            { PS2_KEY_BACK,      PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '|',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // 
            { PS2_KEY_BACK,      PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '\\',         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Back slash maps to Yen
            { PS2_KEY_BTICK,     PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '`',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Pipe
            { PS2_KEY_BTICK,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '|',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Back tick `
            { PS2_KEY_HASH,      PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '~',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Tilde has no mapping.
            { PS2_KEY_HASH,      PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '#',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Hash
            { PS2_KEY_BS,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x08,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Backspace
            { PS2_KEY_ESC,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x1B,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // ESCape
            { PS2_KEY_SCROLL,    PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    ' ',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Not assigned.
            { PS2_KEY_INSERT,    PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    X1KEY_INS,    0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // INSERT
            { PS2_KEY_HOME,      PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    X1KEY_CLR,    0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // CLR
            { PS2_KEY_HOME,      PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    X1KEY_HOME,   0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // HOME
            { PS2_KEY_DELETE,    PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    X1KEY_DEL,    0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // DELETE
            { PS2_KEY_END,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x11,         0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // END
            { PS2_KEY_PGUP,      PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x0E,         0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // Roll Up.
            { PS2_KEY_PGDN,      PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x0F,         0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // Roll Down
            { PS2_KEY_UP_ARROW,  PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    X1KEY_UP,     0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Up Arrow
            { PS2_KEY_L_ARROW,   PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    X1KEY_LEFT,   0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Left Arrow
            { PS2_KEY_DN_ARROW,  PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    X1KEY_DOWN,   0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Down Arrow
            { PS2_KEY_R_ARROW,   PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    X1KEY_RIGHT,  0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Right Arrow
            { PS2_KEY_NUM,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Not assigned.
            // GRPH (Alt Gr)
            //                                                                                                                                ModeB Byte1   ModeB Byte2  ModeB Byte3
            //PS2 Code           PS2 Ctrl (Flags to Match)                                                Machine               X1 Keyb Mode  X1 Data                    X1 Ctrl (Flags to Set).
        	{ PS2_KEY_0,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xFA,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+0
        	{ PS2_KEY_1,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xF1,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+1
        	{ PS2_KEY_2,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xF2,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+2
        	{ PS2_KEY_3,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xF3,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+3
        	{ PS2_KEY_4,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xF4,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+4
        	{ PS2_KEY_5,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xF5,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+5
        	{ PS2_KEY_6,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xF6,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+6
        	{ PS2_KEY_7,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xF7,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+7
        	{ PS2_KEY_8,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xF8,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+8
        	{ PS2_KEY_9,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xF9,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+9
        	{ PS2_KEY_A,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x7F,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+A
        	{ PS2_KEY_B,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x84,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+B
        	{ PS2_KEY_C,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x82,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+C
        	{ PS2_KEY_D,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xEA,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+D
        	{ PS2_KEY_E,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE2,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+E
        	{ PS2_KEY_F,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xEB,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+F
        	{ PS2_KEY_G,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xEC,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+G
        	{ PS2_KEY_H,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xED,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+H
        	{ PS2_KEY_I,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE7,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+I
        	{ PS2_KEY_J,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xEE,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+J
        	{ PS2_KEY_K,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xEF,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+K
        	{ PS2_KEY_L,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x8E,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+L
        	{ PS2_KEY_M,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x86,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+M
        	{ PS2_KEY_N,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x85,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+N
        	{ PS2_KEY_O,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xF0,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+O
        	{ PS2_KEY_P,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x8D,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+P
        	{ PS2_KEY_Q,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE0,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+Q
        	{ PS2_KEY_R,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE3,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+R
        	{ PS2_KEY_S,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE9,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+S
        	{ PS2_KEY_T,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE4,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+T
        	{ PS2_KEY_U,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE6,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+U
        	{ PS2_KEY_V,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x83,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+V
        	{ PS2_KEY_W,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE1,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+W
        	{ PS2_KEY_X,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x81,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+X
        	{ PS2_KEY_Y,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE5,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+Y
        	{ PS2_KEY_Z,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x80,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+Z
            { PS2_KEY_COMMA,     PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x87,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+,
            { PS2_KEY_SEMI,      PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x89,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+;
            { PS2_KEY_DOT,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x88,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+.
            { PS2_KEY_DIV,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xFE,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+/
            { PS2_KEY_MINUS,     PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x8C,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+-
            { PS2_KEY_APOS,      PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x8A,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+'
            { PS2_KEY_OPEN_SQ,   PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xFC,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+[
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xE8,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+]
            { PS2_KEY_BACK,      PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x90,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRPH+Backslash
            { PS2_KEY_KP0,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x8F,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad 0
            { PS2_KEY_KP1,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x99,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad 1
            { PS2_KEY_KP2,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x92,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad 2
            { PS2_KEY_KP3,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x98,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad 3
            { PS2_KEY_KP4,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x95,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad 4
            { PS2_KEY_KP5,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x96,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad 5
            { PS2_KEY_KP6,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x94,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad 6
            { PS2_KEY_KP7,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x9A,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad 7
            { PS2_KEY_KP8,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x93,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad 8
            { PS2_KEY_KP9,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x97,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad 9
            { PS2_KEY_KP_DOT,    PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x91,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad Full stop . 
            { PS2_KEY_KP_PLUS,   PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x9D,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad Plus + 
            { PS2_KEY_KP_MINUS,  PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x9C,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad Minus - 
            { PS2_KEY_KP_TIMES,  PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x9B,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad Times * 
            { PS2_KEY_KP_DIV,    PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x9E,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad Divide /
            { PS2_KEY_KP_ENTER,  PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x90,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // GRPH+Keypad Enter /
            // KANA (Alt)
            //                                                                                                                                ModeB Byte1   ModeB Byte2  ModeB Byte3
            //PS2 Code           PS2 Ctrl (Flags to Match)                                                Machine               X1 Keyb Mode  X1 Data                    X1 Ctrl (Flags to Set).
        	{ PS2_KEY_0,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xA6,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+0
        	{ PS2_KEY_0,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xDC,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+0
        	{ PS2_KEY_1,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xC7,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+1
        	{ PS2_KEY_2,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xCC,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+2
        	{ PS2_KEY_3,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xA7,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+3
        	{ PS2_KEY_3,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xB1,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+3
        	{ PS2_KEY_4,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xA9,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+4
        	{ PS2_KEY_4,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xB3,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+4
        	{ PS2_KEY_5,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xAA,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+5
        	{ PS2_KEY_5,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xB4,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+5
        	{ PS2_KEY_6,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xAB,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+6
        	{ PS2_KEY_6,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xB5,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+6
        	{ PS2_KEY_7,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xAC,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+7
        	{ PS2_KEY_7,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xD4,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+7
        	{ PS2_KEY_8,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xAD,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+8
        	{ PS2_KEY_8,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xD5,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+8
        	{ PS2_KEY_9,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xAE,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+9
        	{ PS2_KEY_9,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xD6,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+9
        	{ PS2_KEY_A,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xC1,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+A
        	{ PS2_KEY_B,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xBA,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+B
        	{ PS2_KEY_C,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xBF,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+C
        	{ PS2_KEY_D,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xBC,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+D
        	{ PS2_KEY_E,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xA8,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+E
        	{ PS2_KEY_E,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xB2,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+E
        	{ PS2_KEY_F,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xCA,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+F
        	{ PS2_KEY_G,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xB7,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+G
        	{ PS2_KEY_H,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xB8,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+H
        	{ PS2_KEY_I,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xC6,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+I
        	{ PS2_KEY_J,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xCF,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+J
        	{ PS2_KEY_K,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xC9,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+K
        	{ PS2_KEY_L,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xD8,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+L
        	{ PS2_KEY_M,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xD3,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+M
        	{ PS2_KEY_N,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xD0,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+N
        	{ PS2_KEY_O,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xD7,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+O
        	{ PS2_KEY_P,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xBE,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+P
        	{ PS2_KEY_Q,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xC0,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+Q
        	{ PS2_KEY_R,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xBD,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+R
        	{ PS2_KEY_S,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xC4,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+S
        	{ PS2_KEY_T,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xB6,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+T
        	{ PS2_KEY_U,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xC5,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+U
        	{ PS2_KEY_V,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xCB,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+V
        	{ PS2_KEY_W,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xC3,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+W
        	{ PS2_KEY_X,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xBB,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+X
        	{ PS2_KEY_Y,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xDD,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+Y
        	{ PS2_KEY_Z,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xAF,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+Z
        	{ PS2_KEY_Z,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xC2,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+Z
            { PS2_KEY_COMMA,     PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xA4,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+,
            { PS2_KEY_COMMA,     PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xC8,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+,
            { PS2_KEY_SEMI,      PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xDA,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+;
            { PS2_KEY_DOT,       PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xA1,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+.
            { PS2_KEY_DOT,       PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xD9,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+.
            { PS2_KEY_DIV,       PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xA5,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+/
            { PS2_KEY_DIV,       PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xD2,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+/
            { PS2_KEY_MINUS,     PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xCE,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+-
            { PS2_KEY_APOS,      PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xDE,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+'
            { PS2_KEY_OPEN_SQ,   PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xA2,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+[
            { PS2_KEY_OPEN_SQ,   PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xDF,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+[
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xA3,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+]
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xD1,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+]
            { PS2_KEY_BACK,      PS2CTRL_KANA,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0xDB,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+Backslash
            { PS2_KEY_BS,        PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x12,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA+SHIFT+Backspace
            // Keypad.
            { PS2_KEY_KP0,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '0',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad 0
            { PS2_KEY_KP1,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '1',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad 1
            { PS2_KEY_KP2,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '2',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad 2
            { PS2_KEY_KP3,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '3',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad 3
            { PS2_KEY_KP4,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '4',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad 4
            { PS2_KEY_KP5,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '5',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad 5
            { PS2_KEY_KP6,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '6',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad 6
            { PS2_KEY_KP7,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '7',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad 7
            { PS2_KEY_KP8,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '8',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad 8
            { PS2_KEY_KP9,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '9',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad 9
            { PS2_KEY_KP_COMMA,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    ',',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad Comma , 
            { PS2_KEY_KP_DOT,    PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '.',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad Full stop . 
            { PS2_KEY_KP_PLUS,   PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '+',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad Plus + 
            { PS2_KEY_KP_MINUS,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '-',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad Minus - 
            { PS2_KEY_KP_TIMES,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '*',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad Times * 
            { PS2_KEY_KP_DIV,    PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '/',          0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad Divide /
            { PS2_KEY_KP_ENTER,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x0D,         0x00,        0xFF & ~(X1_CTRL_TENKEY | X1_CTRL_PRESS),                            },   // Keypad Enter /
            { PS2_KEY_KP_EQUAL,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    '=',          0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Keypad Equal =
            //                                                                                                                                ModeB Byte1   ModeB Byte2  ModeB Byte3
            //PS2 Code           PS2 Ctrl (Flags to Match)                                                Machine               X1 Keyb Mode  X1 Data                    X1 Ctrl (Flags to Set).
            // Special keys.
            { PS2_KEY_PRTSCR,    PS2CTRL_FUNC,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // ARGO KEY
            { PS2_KEY_PAUSE,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x03,         0x00,        0xFF & ~(X1_CTRL_PRESS | X1_CTRL_TENKEY),                            },   // BREAK KEY
            { PS2_KEY_L_GUI,     PS2CTRL_FUNC | PS2CTRL_GUI,                 KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // GRAPH KEY
          //{ PS2_KEY_L_ALT,     PS2CTRL_FUNC | PS2CTRL_KANA,                KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KJ1 Sentence
          //{ PS2_KEY_R_ALT,     PS2CTRL_FUNC | PS2CTRL_GRAPH,               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KJ2 Transform
            { PS2_KEY_R_GUI,     PS2CTRL_FUNC | PS2CTRL_GUI,                 KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // KANA KEY
            { PS2_KEY_MENU,      PS2CTRL_FUNC | PS2CTRL_GUI,                 KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Not assigned.
            // Modifiers are last, only being selected if an earlier match isnt made.
            { PS2_KEY_L_SHIFT,   PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   
            { PS2_KEY_R_SHIFT,   PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   
            { PS2_KEY_L_CTRL,    PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   
            { PS2_KEY_R_CTRL,    PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },   // Map to Control
            { 0,                 PS2CTRL_NONE,                               KEYMAP_STANDARD,             X1_ALL,               X1_MODE_A,    0x00,         0x00,        0xFF & ~(X1_CTRL_PRESS),                                             },
        }};
};

#endif // X1_H
