/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            MZ5665.h
// Created:         Apr 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Header for the Sharp MZ-6500 to HID (PS/2, Bluetooth) interface logic.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Apr 2022 - Initial write.
//            v1.01 Jun 2022 - Updates to reflect changes realised in other modules due to addition of
//                             bluetooth and suspend logic due to NVS issues using both cores.
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

#ifndef MZ5665_H
#define MZ5665_H

// Include the specification class.
#include "KeyInterface.h"
#include "NVS.h"
#include "LED.h"
#include "HID.h"
#include <vector>
#include <map>

// NB: Macros definitions put inside class for clarity, they are still global scope.

// Encapsulate the Sharp MZ-6500 interface.
class MZ5665 : public KeyInterface  {
    // Macros.
    //
    #define NUMELEM(a)                      (sizeof(a)/sizeof(a[0]))
    
    // Constants.
    #define MZ5665IF_VERSION                1.01
    #define MZ5665IF_KEYMAP_FILE            "MZ5665_KeyMap.BIN"
    #define MAX_MZ5665_XMIT_KEY_BUF         16
    #define PS2TBL_MZ5665_MAXROWS           349
    
    // MZ-6500 Key control bit mask.
    #define MZ5665_CTRL_GRAPH               ((unsigned char) (1 << 4))
    #define MZ5665_CTRL_CAPS                ((unsigned char) (1 << 3))
    #define MZ5665_CTRL_KANA                ((unsigned char) (1 << 2))
    #define MZ5665_CTRL_SHIFT               ((unsigned char) (1 << 1))
    #define MZ5665_CTRL_CTRL                ((unsigned char) (1 << 0))

    // Special key definition.
    #define MZ5665_KEY_UP                   0x1E     // ↑
    #define MZ5665_KEY_DOWN                 0x1F     // ↓
    #define MZ5665_KEY_LEFT                 0x1D     // ←
    #define MZ5665_KEY_RIGHT                0x1C     // → →
    #define MZ5665_KEY_INS                  0x12     // INS
    #define MZ5665_KEY_DEL                  0x08     // DEL
    #define MZ5665_KEY_CLR                  0x0C     // CLR
    #define MZ5665_KEY_HOME                 0x0B     // HOME
    
    // PS2 Flag definitions.
    #define PS2CTRL_NONE                    0x00     // No keys active = 0
    #define PS2CTRL_SHIFT                   0x01     // Shfit Key active = 1
    #define PS2CTRL_CTRL                    0x02     // Ctrl Key active = 1
    #define PS2CTRL_CAPS                    0x04     // CAPS active = 1
    #define PS2CTRL_KANA                    0x08     // KANA active = 1
    #define PS2CTRL_GRAPH                   0x10     // GRAPH active = 1
    #define PS2CTRL_GUI                     0x20     // GUI Key active = 1
    #define PS2CTRL_FUNC                    0x40     // Special Function Keys active = 1
    #define PS2CTRL_BREAK                   0x80     // BREAK Key active = 1
    #define PS2CTRL_EXACT                   0x80     // EXACT Match active = 1
    
    // The initial mapping is made inside the PS2KeyAdvanced class from Scan Code Set 2 to ASCII
    // for a selected keyboard. Special functions are detected and combined inside this module
    // before mapping with the table below to extract the MZ-6500 key code and control data.
    // ie. PS/2 Scan Code -> ASCII + Flags -> MZ-6500 Key Code + Ctrl Data

    // Keyboard mapping table column names.
    #define PS2TBL_PS2KEYCODE_NAME          "PS/2 KeyCode"
    #define PS2TBL_PS2CTRL_NAME             "PS/2 Control Key"
    #define PS2TBL_KEYBOARDMODEL_NAME       "For Keyboard"
    #define PS2TBL_MACHINE_NAME             "For Host Model"
    #define PS2TBL_MZ5665_KEYCODE_NAME      "MZ5665 KeyCode"
    #define PS2TBL_MZ5665__CTRL_NAME        "MZ5665 Control Key"

    // Keyboard mapping table column types.
    #define PS2TBL_PS2KEYCODE_TYPE          "hex"
    #define PS2TBL_PS2CTRL_TYPE             "custom_cbp_ps2ctrl"
    #define PS2TBL_KEYBOARDMODEL_TYPE       "custom_cbp_keybmodel"
    #define PS2TBL_MACHINE_TYPE             "custom_cbp_machine"
    #define PS2TBL_MZ5665_KEYCODE_TYPE      "hex"
    #define PS2TBL_MZ5665_CTRL_TYPE         "custom_cbn_x1ctrl"

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
   
    // Keyboard mapping table select list for target machine.
    #define MZ5665_SEL_ALL                  "ALL"

    // Keyboard mapping table select list for MZ5665 Control codes.
    #define MZ5665_CTRL_SEL_GRAPH           "GRAPH"
    #define MZ5665_CTRL_SEL_CAPS            "CAPS"
    #define MZ5665_CTRL_SEL_KANA            "KANA"
    #define MZ5665_CTRL_SEL_SHIFT           "SHIFT"
    #define MZ5665_CTRL_SEL_CTRL            "CTRL"

    // The Sharp MZ-6500 Series was released over a number of years and each iteration added changes/updates. In order to cater for differences, it is possible to assign a key mapping
    // to a specific machine type(s) or all of the series by adding the flags below into the mapping table.
    #define MZ5665_ALL                      0xFF
    
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
                                        MZ5665(void);
                                        MZ5665(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID, const char *fsPath);
                                        MZ5665(NVS *hdlNVS, HID *hdlHID, const char *fsPath);
                                       ~MZ5665(void);
        bool                            createKeyMapFile(std::fstream &outFile);
        bool                            storeDataToKeyMapFile(std::fstream &outFile, char *data, int size);
        bool                            storeDataToKeyMapFile(std::fstream & outFile, std::vector<uint32_t>& dataArray);
        bool                            closeAndCommitKeyMapFile(std::fstream &outFile, bool cleanupOnly);
        std::string                     getKeyMapFileName(void) { return(MZ5665IF_KEYMAP_FILE); };
        void                            getKeyMapHeaders(std::vector<std::string>& headerList);
        void                            getKeyMapTypes(std::vector<std::string>& typeList);
        bool                            getKeyMapSelectList(std::vector<std::pair<std::string, int>>& selectList, std::string option);        
        bool                            getKeyMapData(std::vector<uint32_t>& dataArray, int *row, bool start);

        // Method to return the class version number.
        float version(void)
        {
            return(MZ5665IF_VERSION);
        }

    protected:

    private:
        // Prototypes.
        void                            pushKeyToQueue(uint32_t key);
        IRAM_ATTR static void           mzInterface( void * pvParameters );
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

        // Structure to encapsulate a single key map from PS/2 to MZ-5600/MZ-6500.
        typedef struct {
            uint8_t                     ps2KeyCode;
            uint8_t                     ps2Ctrl;
            uint8_t                     keyboardModel;
            uint8_t                     machine;
            uint8_t                     mzKey;
            uint8_t                     mzCtrl;
        } t_keyMapEntry;

        // Structure to encapsulate the entire static keyboard mapping table.
        typedef struct {
            t_keyMapEntry               kme[PS2TBL_MZ5665_MAXROWS];
        } t_keyMap;

        // Structure to maintain the MZ-5600/MZ-6500 interface configuration data. This data is persisted through powercycles as needed.
        typedef struct {
            struct {
                uint8_t                 activeKeyboardMap;      // Model of keyboard a keymap entry is applicable to.
                uint8_t                 activeMachineModel;     // Machine model a keymap entry is applicable to.
            } params;
        } t_mzConfig;
       
        // Configuration data.
        t_mzConfig                      mzConfig;

        // Structure to manage the control signals signifying the state of the MZ-6500 keyboard.
        typedef struct {
            bool                        optionSelect;           // Flag to indicate a user requested keyboard configuration option is being selected.
            uint8_t                     keyCtrl;                // Keyboard state flag control.

            std::string                 fsPath;                 // Path on the underlying filesystem where storage is mounted and accessible.
            t_keyMapEntry              *kme;                    // Pointer to an array in memory to contain PS2 to MZ-6500 mapping values.
            int                         kmeRows;                // Number of rows in the kme table.
            std::string                 keyMapFileName;         // Name of file where extension or replacement key map entries are stored.
        } t_mzControl;

        // Transmit buffer queue item.
        typedef struct {
            uint32_t                    keyCode;                // 16bit, bits 8:0 represent the key, 9 if CTRL to be sent, 10 if ALT to be sent.
        } t_xmitQueueMessage;

        // Thread handles - one per function, ie. HID interface and host target interface.
        TaskHandle_t                    TaskHostIF = NULL;
        TaskHandle_t                    TaskHIDIF  = NULL;

        // Control structure to control interaction and mapping of keys for the host.
        t_mzControl                     mzCtrl;
       
        // Spin lock mutex to hold a coresied to an uninterruptable method. This only works on dual core ESP32's.
        portMUX_TYPE                    mzMutex;

        //
        // This mapping is for the UK Wyse KB-3926 PS/2 keyboard
        //
        t_keyMap                        PS2toMZ5665 = {
        {
            // HELP
            // COPY
          ////PS2 Code           PS2 Ctrl (Flags to Match)                   Keyboard Model               Machine               MZ5665 Data        MZ5665 Ctrl (Flags to Set).
          //{ PS2_KEY_F1,        PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           'v',               0x00,                                                                    },   // SHIFT+F1
          //{ PS2_KEY_F2,        PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           'w',               0x00,                                                                    },   // SHIFT+F2
          //{ PS2_KEY_F3,        PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           'x',               0x00,                                                                    },   // SHIFT+F3
          //{ PS2_KEY_F4,        PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           'y',               0x00,                                                                    },   // SHIFT+F4
          //{ PS2_KEY_F5,        PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           'z',               0x00,                                                                    },   // SHIFT+F5
          //{ PS2_KEY_F1,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           'q',               0x00,                                                                    },   // F1
          //{ PS2_KEY_F2,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           'r',               0x00,                                                                    },   // F2
          //{ PS2_KEY_F3,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           's',               0x00,                                                                    },   // F3
          //{ PS2_KEY_F4,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           't',               0x00,                                                                    },   // F4
          //{ PS2_KEY_F5,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           'u',               0x00,                                                                    },   // F5
          //{ PS2_KEY_F6,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xEC,              0x00,                                                                    },   // F6
          //{ PS2_KEY_F7,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xEB,              0x00,                                                                    },   // F7
          //{ PS2_KEY_F8,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xE2,              0x00,                                                                    },   // F8
          //{ PS2_KEY_F9,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xE1,              0x00,                                                                    },   // F9
          //{ PS2_KEY_F10,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   // XFER
          //{ PS2_KEY_F11,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xFE,              0x00,                                                                    },   // HELP
          //{ PS2_KEY_F12,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   // COPY
          //{ PS2_KEY_TAB,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x09,              0x00,                                                                    },   // TAB
            // Numeric keys.
        	{ PS2_KEY_0,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '0',               0x00,                                                                    },   // 0
        	{ PS2_KEY_1,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '1',               0x00,                                                                    },   // 1
        	{ PS2_KEY_2,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '2',               0x00,                                                                    },   // 2
        	{ PS2_KEY_3,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '3',               0x00,                                                                    },   // 3
        	{ PS2_KEY_4,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '4',               0x00,                                                                    },   // 4
        	{ PS2_KEY_5,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '5',               0x00,                                                                    },   // 5
        	{ PS2_KEY_6,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '6',               0x00,                                                                    },   // 6
        	{ PS2_KEY_7,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '7',               0x00,                                                                    },   // 7
        	{ PS2_KEY_8,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '8',               0x00,                                                                    },   // 8
        	{ PS2_KEY_9,         PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '9',               0x00,                                                                    },   // 9
            // Punctuation keys.
        	{ PS2_KEY_0,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           ')',               0x00,                                                                    },   // Close Right Bracket )
        	{ PS2_KEY_1,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '!',               0x00,                                                                    },   // Exclamation
        	{ PS2_KEY_2,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '"',               0x00,                                                                    },   // Double quote.
            { PS2_KEY_3,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x23,              0x00,                                                                    },   // Pound Sign -> Hash
        	{ PS2_KEY_4,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '$',               0x00,                                                                    },   // Dollar
        	{ PS2_KEY_5,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '%',               0x00,                                                                    },   // Percent
        	{ PS2_KEY_6,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '^',               0x00,                                                                    },   // Kappa
        	{ PS2_KEY_7,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '&',               0x00,                                                                    },   // Ampersand
        	{ PS2_KEY_8,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '*',               0x00,                                                                    },   // Star
        	{ PS2_KEY_9,         PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '(',               0x00,                                                                    },   // Open Left Bracket (
            // ALPHA keys, lower and uppercase.        
            //PS2 Code           PS2 Ctrl (Flags to Match)                   Keyboard Model               Machine               MZ5665 Data        MZ5665 Ctrl (Flags to Set).
        	{ PS2_KEY_A,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'a',               0x00,                                                                    },   // a
        	{ PS2_KEY_A,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'A',               0x00,                                                                    },   // A
            { PS2_KEY_B,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'b',               0x00,                                                                    },   // b
            { PS2_KEY_B,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'B',               0x00,                                                                    },   // B
            { PS2_KEY_C,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'c',               0x00,                                                                    },   // c
            { PS2_KEY_C,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'C',               0x00,                                                                    },   // C
            { PS2_KEY_D,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'd',               0x00,                                                                    },   // d
            { PS2_KEY_D,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'D',               0x00,                                                                    },   // D
            { PS2_KEY_E,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'e',               0x00,                                                                    },   // e
            { PS2_KEY_E,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'E',               0x00,                                                                    },   // E
            { PS2_KEY_F,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'f',               0x00,                                                                    },   // f
            { PS2_KEY_F,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'F',               0x00,                                                                    },   // F
            { PS2_KEY_G,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'g',               0x00,                                                                    },   // g
            { PS2_KEY_G,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'G',               0x00,                                                                    },   // G
            { PS2_KEY_H,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'h',               0x00,                                                                    },   // h
            { PS2_KEY_H,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'H',               0x00,                                                                    },   // H
            { PS2_KEY_I,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'i',               0x00,                                                                    },   // i
            { PS2_KEY_I,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'I',               0x00,                                                                    },   // I
            { PS2_KEY_J,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'j',               0x00,                                                                    },   // j
            { PS2_KEY_J,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'J',               0x00,                                                                    },   // J
            { PS2_KEY_K,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'k',               0x00,                                                                    },   // k
            { PS2_KEY_K,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'K',               0x00,                                                                    },   // K
            { PS2_KEY_L,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'l',               0x00,                                                                    },   // l
            { PS2_KEY_L,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'L',               0x00,                                                                    },   // L
            { PS2_KEY_M,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'm',               0x00,                                                                    },   // m
            { PS2_KEY_M,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'M',               0x00,                                                                    },   // M
            { PS2_KEY_N,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'n',               0x00,                                                                    },   // n
            { PS2_KEY_N,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'N',               0x00,                                                                    },   // N
            { PS2_KEY_O,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'o',               0x00,                                                                    },   // o
            { PS2_KEY_O,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'O',               0x00,                                                                    },   // O
            { PS2_KEY_P,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'p',               0x00,                                                                    },   // p
            { PS2_KEY_P,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'P',               0x00,                                                                    },   // P
            { PS2_KEY_Q,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'q',               0x00,                                                                    },   // q
            { PS2_KEY_Q,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'Q',               0x00,                                                                    },   // Q
            { PS2_KEY_R,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'r',               0x00,                                                                    },   // r
            { PS2_KEY_R,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'R',               0x00,                                                                    },   // R
            { PS2_KEY_S,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           's',               0x00,                                                                    },   // s
            { PS2_KEY_S,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'S',               0x00,                                                                    },   // S
            { PS2_KEY_T,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           't',               0x00,                                                                    },   // t
            { PS2_KEY_T,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'T',               0x00,                                                                    },   // T
            { PS2_KEY_U,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'u',               0x00,                                                                    },   // u
            { PS2_KEY_U,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'U',               0x00,                                                                    },   // U
            { PS2_KEY_V,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'v',               0x00,                                                                    },   // v
            { PS2_KEY_V,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'V',               0x00,                                                                    },   // V
            { PS2_KEY_W,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'w',               0x00,                                                                    },   // w
            { PS2_KEY_W,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'W',               0x00,                                                                    },   // W
            { PS2_KEY_X,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'x',               0x00,                                                                    },   // x
            { PS2_KEY_X,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'X',               0x00,                                                                    },   // X
            { PS2_KEY_Y,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'y',               0x00,                                                                    },   // y
            { PS2_KEY_Y,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'Y',               0x00,                                                                    },   // Y
            { PS2_KEY_Z,         PS2CTRL_SHIFT | PS2CTRL_CAPS,               KEYMAP_STANDARD,             MZ5665_ALL,           'z',               0x00,                                                                    },   // z
            { PS2_KEY_Z,         PS2CTRL_CAPS,                               KEYMAP_STANDARD,             MZ5665_ALL,           'Z',               0x00,                                                                    },   // Z
    
            //PS2 Code           PS2 Ctrl (Flags to Match)                   Keyboard Model               Machine               MZ5665 Data        MZ5665 Ctrl (Flags to Set).
            { PS2_KEY_SPACE,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           ' ',               0x00,                                                                    },   // Space
            { PS2_KEY_COMMA,     PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '<',               0x00,                                                                    },   // Less Than <
            { PS2_KEY_COMMA,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           ',',               0x00,                                                                    },   // Comma ,
            { PS2_KEY_SEMI,      PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           ':',               0x00,                                                                    },   // Colon :
            { PS2_KEY_SEMI,      PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           ';',               0x00,                                                                    },   // Semi-Colon ;
            { PS2_KEY_DOT,       PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '>',               0x00,                                                                    },   // Greater Than >
            { PS2_KEY_DOT,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '.',               0x00,                                                                    },   // Full stop .
            { PS2_KEY_DIV,       PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '?',               0x00,                                                                    },   // Question ?
            { PS2_KEY_DIV,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '/',               0x00,                                                                    },   // Divide /
            { PS2_KEY_MINUS,     PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '_',               0x00,                                                                    },   // Underscore
            { PS2_KEY_MINUS,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '-',               0x00,                                                                    },   
            { PS2_KEY_APOS,      PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '@',               0x00,                                                                    },   // At @
            { PS2_KEY_APOS,      PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '\'',              0x00,                                                                    },   // Single quote '
            { PS2_KEY_OPEN_SQ,   PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '{',               0x00,                                                                    },   // Open Left Brace {
            { PS2_KEY_OPEN_SQ,   PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '[',               0x00,                                                                    },   // Open Left Square Bracket [
            { PS2_KEY_EQUAL,     PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '+',               0x00,                                                                    },   // Plus +
            { PS2_KEY_EQUAL,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '=',               0x00,                                                                    },   // Equal =
            { PS2_KEY_CAPS,      PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           ' ',               0x00,                                                                    },   // LOCK
            { PS2_KEY_ENTER,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x0D,              0x00,                                                                    },   // ENTER/RETURN
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '}',               0x00,                                                                    },   // Close Right Brace }
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           ']',               0x00,                                                                    },   // Close Right Square Bracket ]
            { PS2_KEY_BACK,      PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '|',               0x00,                                                                    },   // 
            { PS2_KEY_BACK,      PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '\\',              0x00,                                                                    },   // Back slash maps to Yen
            { PS2_KEY_BTICK,     PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '`',               0x00,                                                                    },   // Pipe
            { PS2_KEY_BTICK,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '|',               0x00,                                                                    },   // Back tick `
            { PS2_KEY_HASH,      PS2CTRL_SHIFT,                              KEYMAP_STANDARD,             MZ5665_ALL,           '~',               0x00,                                                                    },   // Tilde has no mapping.
            { PS2_KEY_HASH,      PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '#',               0x00,                                                                    },   // Hash
            { PS2_KEY_BS,        PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x08,              0x00,                                                                    },   // Backspace
            { PS2_KEY_ESC,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x1B,              0x00,                                                                    },   // ESCape
            { PS2_KEY_SCROLL,    PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           ' ',               0x00,                                                                    },   // Not assigned.
            { PS2_KEY_INSERT,    PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           MZ5665_KEY_INS,    0x00,                                                                    },   // INSERT
            { PS2_KEY_HOME,      PS2CTRL_FUNC | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           MZ5665_KEY_CLR,    0x00,                                                                    },   // CLR
            { PS2_KEY_HOME,      PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           MZ5665_KEY_HOME,   0x00,                                                                    },   // HOME
            { PS2_KEY_DELETE,    PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           MZ5665_KEY_DEL,    0x00,                                                                    },   // DELETE
            { PS2_KEY_END,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x11,              0x00,                                                                    },   // END
            { PS2_KEY_PGUP,      PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x0E,              0x00,                                                                    },   // Roll Up.
            { PS2_KEY_PGDN,      PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x0F,              0x00,                                                                    },   // Roll Down
            { PS2_KEY_UP_ARROW,  PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           MZ5665_KEY_UP,     0x00,                                                                    },   // Up Arrow
            { PS2_KEY_L_ARROW,   PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           MZ5665_KEY_LEFT,   0x00,                                                                    },   // Left Arrow
            { PS2_KEY_DN_ARROW,  PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           MZ5665_KEY_DOWN,   0x00,                                                                    },   // Down Arrow
            { PS2_KEY_R_ARROW,   PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           MZ5665_KEY_RIGHT,  0x00,                                                                    },   // Right Arrow
            { PS2_KEY_NUM,       PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   // Not assigned.
            // GRPH (Alt Gr)
            //PS2 Code           PS2 Ctrl (Flags to Match)                   Keyboard Model               Machine               MZ5665 Data        MZ5665 Ctrl (Flags to Set).
        	{ PS2_KEY_0,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xFA,              0x00,                                                                    },   // GRPH+0
        	{ PS2_KEY_1,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xF1,              0x00,                                                                    },   // GRPH+1
        	{ PS2_KEY_2,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xF2,              0x00,                                                                    },   // GRPH+2
        	{ PS2_KEY_3,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xF3,              0x00,                                                                    },   // GRPH+3
        	{ PS2_KEY_4,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xF4,              0x00,                                                                    },   // GRPH+4
        	{ PS2_KEY_5,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xF5,              0x00,                                                                    },   // GRPH+5
        	{ PS2_KEY_6,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xF6,              0x00,                                                                    },   // GRPH+6
        	{ PS2_KEY_7,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xF7,              0x00,                                                                    },   // GRPH+7
        	{ PS2_KEY_8,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xF8,              0x00,                                                                    },   // GRPH+8
        	{ PS2_KEY_9,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xF9,              0x00,                                                                    },   // GRPH+9
        	{ PS2_KEY_A,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x7F,              0x00,                                                                    },   // GRPH+A
        	{ PS2_KEY_B,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x84,              0x00,                                                                    },   // GRPH+B
        	{ PS2_KEY_C,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x82,              0x00,                                                                    },   // GRPH+C
        	{ PS2_KEY_D,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xEA,              0x00,                                                                    },   // GRPH+D
        	{ PS2_KEY_E,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xE2,              0x00,                                                                    },   // GRPH+E
        	{ PS2_KEY_F,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xEB,              0x00,                                                                    },   // GRPH+F
        	{ PS2_KEY_G,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xEC,              0x00,                                                                    },   // GRPH+G
        	{ PS2_KEY_H,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xED,              0x00,                                                                    },   // GRPH+H
        	{ PS2_KEY_I,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xE7,              0x00,                                                                    },   // GRPH+I
        	{ PS2_KEY_J,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xEE,              0x00,                                                                    },   // GRPH+J
        	{ PS2_KEY_K,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xEF,              0x00,                                                                    },   // GRPH+K
        	{ PS2_KEY_L,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x8E,              0x00,                                                                    },   // GRPH+L
        	{ PS2_KEY_M,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x86,              0x00,                                                                    },   // GRPH+M
        	{ PS2_KEY_N,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x85,              0x00,                                                                    },   // GRPH+N
        	{ PS2_KEY_O,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xF0,              0x00,                                                                    },   // GRPH+O
        	{ PS2_KEY_P,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x8D,              0x00,                                                                    },   // GRPH+P
        	{ PS2_KEY_Q,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xE0,              0x00,                                                                    },   // GRPH+Q
        	{ PS2_KEY_R,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xE3,              0x00,                                                                    },   // GRPH+R
        	{ PS2_KEY_S,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xE9,              0x00,                                                                    },   // GRPH+S
        	{ PS2_KEY_T,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xE4,              0x00,                                                                    },   // GRPH+T
        	{ PS2_KEY_U,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xE6,              0x00,                                                                    },   // GRPH+U
        	{ PS2_KEY_V,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x83,              0x00,                                                                    },   // GRPH+V
        	{ PS2_KEY_W,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xE1,              0x00,                                                                    },   // GRPH+W
        	{ PS2_KEY_X,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x81,              0x00,                                                                    },   // GRPH+X
        	{ PS2_KEY_Y,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xE5,              0x00,                                                                    },   // GRPH+Y
        	{ PS2_KEY_Z,         PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x80,              0x00,                                                                    },   // GRPH+Z
            { PS2_KEY_COMMA,     PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x87,              0x00,                                                                    },   // GRPH+,
            { PS2_KEY_SEMI,      PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x89,              0x00,                                                                    },   // GRPH+;
            { PS2_KEY_DOT,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x88,              0x00,                                                                    },   // GRPH+.
            { PS2_KEY_DIV,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xFE,              0x00,                                                                    },   // GRPH+/
            { PS2_KEY_MINUS,     PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x8C,              0x00,                                                                    },   // GRPH+-
            { PS2_KEY_APOS,      PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x8A,              0x00,                                                                    },   // GRPH+'
            { PS2_KEY_OPEN_SQ,   PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xFC,              0x00,                                                                    },   // GRPH+[
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0xE8,              0x00,                                                                    },   // GRPH+]
            { PS2_KEY_BACK,      PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x90,              0x00,                                                                    },   // GRPH+Backslash
            { PS2_KEY_KP0,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x8F,              0x00,                                                                    },   // GRPH+Keypad 0
            { PS2_KEY_KP1,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x99,              0x00,                                                                    },   // GRPH+Keypad 1
            { PS2_KEY_KP2,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x92,              0x00,                                                                    },   // GRPH+Keypad 2
            { PS2_KEY_KP3,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x98,              0x00,                                                                    },   // GRPH+Keypad 3
            { PS2_KEY_KP4,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x95,              0x00,                                                                    },   // GRPH+Keypad 4
            { PS2_KEY_KP5,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x96,              0x00,                                                                    },   // GRPH+Keypad 5
            { PS2_KEY_KP6,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x94,              0x00,                                                                    },   // GRPH+Keypad 6
            { PS2_KEY_KP7,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x9A,              0x00,                                                                    },   // GRPH+Keypad 7
            { PS2_KEY_KP8,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x93,              0x00,                                                                    },   // GRPH+Keypad 8
            { PS2_KEY_KP9,       PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x97,              0x00,                                                                    },   // GRPH+Keypad 9
            { PS2_KEY_KP_DOT,    PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x91,              0x00,                                                                    },   // GRPH+Keypad Full stop . 
            { PS2_KEY_KP_PLUS,   PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x9D,              0x00,                                                                    },   // GRPH+Keypad Plus + 
            { PS2_KEY_KP_MINUS,  PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x9C,              0x00,                                                                    },   // GRPH+Keypad Minus - 
            { PS2_KEY_KP_TIMES,  PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x9B,              0x00,                                                                    },   // GRPH+Keypad Times * 
            { PS2_KEY_KP_DIV,    PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x9E,              0x00,                                                                    },   // GRPH+Keypad Divide /
            { PS2_KEY_KP_ENTER,  PS2CTRL_GRAPH,                              KEYMAP_STANDARD,             MZ5665_ALL,           0x90,              0x00,                                                                    },   // GRPH+Keypad Ebter /
            // KANA (Alt)
            //PS2 Code           PS2 Ctrl (Flags to Match)                   Keyboard Model               Machine               MZ5665 Data        MZ5665 Ctrl (Flags to Set).
        	{ PS2_KEY_0,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xA6,              0x00,                                                                    },   // KANA+SHIFT+0
        	{ PS2_KEY_0,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xDC,              0x00,                                                                    },   // KANA+0
        	{ PS2_KEY_1,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xC7,              0x00,                                                                    },   // KANA+1
        	{ PS2_KEY_2,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xCC,              0x00,                                                                    },   // KANA+2
        	{ PS2_KEY_3,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xA7,              0x00,                                                                    },   // KANA+SHIFT+3
        	{ PS2_KEY_3,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xB1,              0x00,                                                                    },   // KANA+3
        	{ PS2_KEY_4,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xA9,              0x00,                                                                    },   // KANA+SHIFT+4
        	{ PS2_KEY_4,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xB3,              0x00,                                                                    },   // KANA+4
        	{ PS2_KEY_5,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xAA,              0x00,                                                                    },   // KANA+SHIFT+5
        	{ PS2_KEY_5,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xB4,              0x00,                                                                    },   // KANA+5
        	{ PS2_KEY_6,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xAB,              0x00,                                                                    },   // KANA+SHIFT+6
        	{ PS2_KEY_6,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xB5,              0x00,                                                                    },   // KANA+6
        	{ PS2_KEY_7,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xAC,              0x00,                                                                    },   // KANA+SHIFT+7
        	{ PS2_KEY_7,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xD4,              0x00,                                                                    },   // KANA+7
        	{ PS2_KEY_8,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xAD,              0x00,                                                                    },   // KANA+SHIFT+8
        	{ PS2_KEY_8,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xD5,              0x00,                                                                    },   // KANA+8
        	{ PS2_KEY_9,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xAE,              0x00,                                                                    },   // KANA+SHIFT+9
        	{ PS2_KEY_9,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xD6,              0x00,                                                                    },   // KANA+9
        	{ PS2_KEY_A,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xC1,              0x00,                                                                    },   // KANA+A
        	{ PS2_KEY_B,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xBA,              0x00,                                                                    },   // KANA+B
        	{ PS2_KEY_C,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xBF,              0x00,                                                                    },   // KANA+C
        	{ PS2_KEY_D,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xBC,              0x00,                                                                    },   // KANA+D
        	{ PS2_KEY_E,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xA8,              0x00,                                                                    },   // KANA+SHIFT+E
        	{ PS2_KEY_E,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xB2,              0x00,                                                                    },   // KANA+E
        	{ PS2_KEY_F,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xCA,              0x00,                                                                    },   // KANA+F
        	{ PS2_KEY_G,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xB7,              0x00,                                                                    },   // KANA+G
        	{ PS2_KEY_H,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xB8,              0x00,                                                                    },   // KANA+H
        	{ PS2_KEY_I,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xC6,              0x00,                                                                    },   // KANA+I
        	{ PS2_KEY_J,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xCF,              0x00,                                                                    },   // KANA+J
        	{ PS2_KEY_K,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xC9,              0x00,                                                                    },   // KANA+K
        	{ PS2_KEY_L,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xD8,              0x00,                                                                    },   // KANA+L
        	{ PS2_KEY_M,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xD3,              0x00,                                                                    },   // KANA+M
        	{ PS2_KEY_N,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xD0,              0x00,                                                                    },   // KANA+N
        	{ PS2_KEY_O,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xD7,              0x00,                                                                    },   // KANA+O
        	{ PS2_KEY_P,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xBE,              0x00,                                                                    },   // KANA+P
        	{ PS2_KEY_Q,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xC0,              0x00,                                                                    },   // KANA+Q
        	{ PS2_KEY_R,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xBD,              0x00,                                                                    },   // KANA+R
        	{ PS2_KEY_S,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xC4,              0x00,                                                                    },   // KANA+S
        	{ PS2_KEY_T,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xB6,              0x00,                                                                    },   // KANA+T
        	{ PS2_KEY_U,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xC5,              0x00,                                                                    },   // KANA+U
        	{ PS2_KEY_V,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xCB,              0x00,                                                                    },   // KANA+V
        	{ PS2_KEY_W,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xC3,              0x00,                                                                    },   // KANA+W
        	{ PS2_KEY_X,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xBB,              0x00,                                                                    },   // KANA+X
        	{ PS2_KEY_Y,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xDD,              0x00,                                                                    },   // KANA+Y
        	{ PS2_KEY_Z,         PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xAF,              0x00,                                                                    },   // KANA+SHIFT+Z
        	{ PS2_KEY_Z,         PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xC2,              0x00,                                                                    },   // KANA+Z
            { PS2_KEY_COMMA,     PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xA4,              0x00,                                                                    },   // KANA+SHIFT+,
            { PS2_KEY_COMMA,     PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xC8,              0x00,                                                                    },   // KANA+,
            { PS2_KEY_SEMI,      PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xDA,              0x00,                                                                    },   // KANA+;
            { PS2_KEY_DOT,       PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xA1,              0x00,                                                                    },   // KANA+SHIFT+.
            { PS2_KEY_DOT,       PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xD9,              0x00,                                                                    },   // KANA+.
            { PS2_KEY_DIV,       PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xA5,              0x00,                                                                    },   // KANA+SHIFT+/
            { PS2_KEY_DIV,       PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xD2,              0x00,                                                                    },   // KANA+/
            { PS2_KEY_MINUS,     PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xCE,              0x00,                                                                    },   // KANA+-
            { PS2_KEY_APOS,      PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xDE,              0x00,                                                                    },   // KANA+'
            { PS2_KEY_OPEN_SQ,   PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xA2,              0x00,                                                                    },   // KANA+SHIFT+[
            { PS2_KEY_OPEN_SQ,   PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xDF,              0x00,                                                                    },   // KANA+[
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0xA3,              0x00,                                                                    },   // KANA+SHIFT+]
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xD1,              0x00,                                                                    },   // KANA+]
            { PS2_KEY_BACK,      PS2CTRL_KANA,                               KEYMAP_STANDARD,             MZ5665_ALL,           0xDB,              0x00,                                                                    },   // KANA+Backslash
            { PS2_KEY_BS,        PS2CTRL_KANA | PS2CTRL_SHIFT,               KEYMAP_STANDARD,             MZ5665_ALL,           0x12,              0x00,                                                                    },   // KANA+SHIFT+Backspace
            // Keypad.
            { PS2_KEY_KP0,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '0',               0x00,                                                                    },   // Keypad 0
            { PS2_KEY_KP1,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '1',               0x00,                                                                    },   // Keypad 1
            { PS2_KEY_KP2,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '2',               0x00,                                                                    },   // Keypad 2
            { PS2_KEY_KP3,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '3',               0x00,                                                                    },   // Keypad 3
            { PS2_KEY_KP4,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '4',               0x00,                                                                    },   // Keypad 4
            { PS2_KEY_KP5,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '5',               0x00,                                                                    },   // Keypad 5
            { PS2_KEY_KP6,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '6',               0x00,                                                                    },   // Keypad 6
            { PS2_KEY_KP7,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '7',               0x00,                                                                    },   // Keypad 7
            { PS2_KEY_KP8,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '8',               0x00,                                                                    },   // Keypad 8
            { PS2_KEY_KP9,       PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '9',               0x00,                                                                    },   // Keypad 9
            { PS2_KEY_KP_COMMA,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           ',',               0x00,                                                                    },   // Keypad Comma , 
            { PS2_KEY_KP_DOT,    PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '.',               0x00,                                                                    },   // Keypad Full stop . 
            { PS2_KEY_KP_PLUS,   PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '+',               0x00,                                                                    },   // Keypad Plus + 
            { PS2_KEY_KP_MINUS,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '-',               0x00,                                                                    },   // Keypad Minus - 
            { PS2_KEY_KP_TIMES,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '*',               0x00,                                                                    },   // Keypad Times * 
            { PS2_KEY_KP_DIV,    PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           '/',               0x00,                                                                    },   // Keypad Divide /
            { PS2_KEY_KP_ENTER,  PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x0D,              0x00,                                                                    },   // Keypad Ebter /
            //PS2 Code           PS2 Ctrl (Flags to Match)                   Keyboard Model               Machine               MZ5665 Data        MZ5665 Ctrl (Flags to Set).
            // Special keys.
            { PS2_KEY_PRTSCR,    PS2CTRL_FUNC,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   // ARGO KEY
            { PS2_KEY_PAUSE,     PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x03,              0x00,                                                                    },   // BREAK KEY
            { PS2_KEY_L_GUI,     PS2CTRL_FUNC | PS2CTRL_GUI,                 KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   // GRAPH KEY
          //{ PS2_KEY_L_ALT,     PS2CTRL_FUNC | PS2CTRL_KANA,                KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   // KJ1 Sentence
          //{ PS2_KEY_R_ALT,     PS2CTRL_FUNC | PS2CTRL_GRAPH,               KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   // KJ2 Transform
            { PS2_KEY_R_GUI,     PS2CTRL_FUNC | PS2CTRL_GUI,                 KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   // KANA KEY
            { PS2_KEY_MENU,      PS2CTRL_FUNC | PS2CTRL_GUI,                 KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   // Not assigned.
            // Modifiers are last, only being selected if an earlier match isnt made.
            { PS2_KEY_L_SHIFT,   PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   
            { PS2_KEY_R_SHIFT,   PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   
            { PS2_KEY_L_CTRL,    PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   
            { PS2_KEY_R_CTRL,    PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },   // Map to Control
            { 0,                 PS2CTRL_NONE,                               KEYMAP_STANDARD,             MZ5665_ALL,           0x00,              0x00,                                                                    },
        }};
};

#endif // MZ5665_H
