/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            X68K.h
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Header for the Sharp X68000 to PS/2 interface logic class.
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

#ifndef X68K_H
#define X68K_H

// Include the specification class.
#include "KeyInterface.h"
#include "NVS.h"
#include "LED.h"
#include "HID.h"
#include <vector>
#include <map>

// NB: Macros definitions put inside class for clarity, they are still global scope.

// Encapsulate the Sharp X68K interface.
class X68K : public KeyInterface  {
    // Macros.
    //
    #define NUMELEM(a)                      (sizeof(a)/sizeof(a[0]))
    
    // Constants.
    #define X68KIF_VERSION                  1.03   
    #define X68KIF_KEYMAP_FILE              "X68K_KeyMap.BIN"
    #define MAX_X68K_XMIT_KEY_BUF           16
    #define MAX_X68K_RCV_KEY_BUF            16
    
    // PS2 Flag definitions.
    #define PS2CTRL_NONE                    0x00                                      // No keys active = 0
    #define PS2CTRL_SHIFT                   0x01                                      // Shfit Key active = 1
    #define PS2CTRL_CTRL                    0x02                                      // Ctrl Key active = 1
    #define PS2CTRL_CAPS                    0x04                                      // CAPS active = 1
    #define PS2CTRL_R_CTRL                  0x08                                      // ALT flag used as Right CTRL flag, active = 1
    #define PS2CTRL_ALTGR                   0x10                                      // ALTGR active = 1
    #define PS2CTRL_GUI                     0x20                                      // GUI Key active = 1
    #define PS2CTRL_FUNC                    0x40                                      // Special Function Keys active = 1
    #define PS2CTRL_BREAK                   0x80                                      // BREAK Key active = 1
    #define PS2CTRL_EXACT                   0x80                                      // EXACT Match active = 1
    
    // The initial mapping is made inside the PS2KeyAdvanced class from Scan Code Set 2 to ASCII
    // for a selected keyboard. Special functions are detected and combined inside this module
    // before mapping with the table below to extract the X68K key code and control data.
    // ie. PS/2 Scan Code -> ASCII + Flags -> X68K Key Code + Ctrl Data
    #define PS2TBL_X68K_MAXCOLS             6
    #define PS2TBL_X68K_MAXROWS             131

    // Keyboard mapping table column names.
    #define PS2TBL_PS2KEYCODE_NAME          "PS/2 KeyCode"
    #define PS2TBL_PS2CTRL_NAME             "PS/2 Control Key"
    #define PS2TBL_KEYBOARDMODEL_NAME       "For Keyboard"
    #define PS2TBL_MACHINE_NAME             "For Host Model"
    #define PS2TBL_X68KKEYCODE_NAME         "X68K KeyCode"
    #define PS2TBL_X68KCTRL_NAME            "X68K Control Key"

    // Keyboard mapping table column types.
    #define PS2TBL_PS2KEYCODE_TYPE          "hex"
    #define PS2TBL_PS2CTRL_TYPE             "custom_cbp_ps2ctrl"
    #define PS2TBL_KEYBOARDMODEL_TYPE       "custom_cbp_keybmodel"
    #define PS2TBL_MACHINE_TYPE             "custom_cbp_machine"
    #define PS2TBL_X68KKEYCODE_TYPE         "hex"
    #define PS2TBL_X68KCTRL_TYPE            "custom_cbp_x68kctrl"

    // Keyboard mapping table select list for PS2CTRL.
    #define PS2TBL_PS2CTRL_SEL_NONE         "NONE"
    #define PS2TBL_PS2CTRL_SEL_SHIFT        "SHIFT"
    #define PS2TBL_PS2CTRL_SEL_CTRL         "CTRL"
    #define PS2TBL_PS2CTRL_SEL_CAPS         "CAPS"
    #define PS2TBL_PS2CTRL_SEL_R_CTRL       "RCTRL"
    #define PS2TBL_PS2CTRL_SEL_ALTGR        "ALTGR"
    #define PS2TBL_PS2CTRL_SEL_GUI          "GUI"
    #define PS2TBL_PS2CTRL_SEL_FUNC         "FUNC"
    #define PS2TBL_PS2CTRL_SEL_EXACT        "EXACT"
   
    // Keyboard mapping table select list for target machine.
    #define X68K_SEL_ALL                    "ALL"
    #define X68K_SEL_ORIG                   "ORIGINAL"
    #define X68K_SEL_ACE                    "ACE"
    #define X68K_SEL_EXPERT                 "EXPERT"
    #define X68K_SEL_PRO                    "PRO"
    #define X68K_SEL_SUPER                  "SUPER"
    #define X68K_SEL_XVI                    "XVI"
    #define X68K_SEL_COMPACT                "COMPACT"
    #define X68K_SEL_X68030                 "68030"

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

    // Keyboard mapping table select list for X68K Control codes.
    #define X68K_CTRL_SEL_NONE              "NONE"
    #define X68K_CTRL_SEL_SHIFT             "SHIFT"
    #define X68K_CTRL_SEL_RELEASESHIFT      "RELEASESHIFT"
    #define X68K_CTRL_SEL_R_CTRL            "RCTRL"
  
    // X68K Key control bit mask.
    #define X68K_CTRL_SHIFT                 ((unsigned char) (1 << 7))
    #define X68K_CTRL_RELEASESHIFT          ((unsigned char) (1 << 6))
    #define X68K_CTRL_R_CTRL                ((unsigned char) (1 << 0))
    #define X68K_CTRL_NONE                  0x00
    
    // The Sharp X68000 Series was released over a number of years with several iterations containing changes/updates. Generally Sharp kept the X68000 compatible through the range but just in case
    // differences are found, it is possible to assign a key mapping to a specific machine type(s) or all of the series by adding the flags below into the mapping table.
    #define X68K_ALL                        0xFF
    #define X68K_ORIG                       0x01
    #define X68K_ACE                        0x02
    #define X68K_EXPERT                     0x04
    #define X68K_PRO                        0x08
    #define X68K_SUPER                      0x10
    #define X68K_XVI                        0x20
    #define X68K_COMPACT                    0x40
    #define X68K_X68030                     0x80

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

    // X68000 Scan codes - PS2 codes along with function keys (SHIFT, CTRL etc) are mapped to the X68000 scan codes below.
    #define X68K_KEY_NULL                   0x00  
    #define X68K_KEY_ESC                    0x01  
    #define X68K_KEY_0                      0x0B  
    #define X68K_KEY_1                      0x02  
    #define X68K_KEY_2                      0x03  
    #define X68K_KEY_3                      0x04  
    #define X68K_KEY_4                      0x05  
    #define X68K_KEY_5                      0x06  
    #define X68K_KEY_6                      0x07  
    #define X68K_KEY_7                      0x08  
    #define X68K_KEY_8                      0x09  
    #define X68K_KEY_9                      0x0A  
    #define X68K_KEY_A                      0x1E  
    #define X68K_KEY_B                      0x2E  
    #define X68K_KEY_C                      0x2C  
    #define X68K_KEY_D                      0x20  
    #define X68K_KEY_E                      0x13  
    #define X68K_KEY_F                      0x21  
    #define X68K_KEY_G                      0x22  
    #define X68K_KEY_H                      0x23  
    #define X68K_KEY_I                      0x18  
    #define X68K_KEY_J                      0x24  
    #define X68K_KEY_K                      0x25  
    #define X68K_KEY_L                      0x26  
    #define X68K_KEY_M                      0x30  
    #define X68K_KEY_N                      0x2F  
    #define X68K_KEY_O                      0x19  
    #define X68K_KEY_P                      0x1A  
    #define X68K_KEY_Q                      0x11  
    #define X68K_KEY_R                      0x14  
    #define X68K_KEY_S                      0x1F  
    #define X68K_KEY_T                      0x15  
    #define X68K_KEY_U                      0x17  
    #define X68K_KEY_V                      0x2D  
    #define X68K_KEY_W                      0x12  
    #define X68K_KEY_X                      0x2B  
    #define X68K_KEY_Y                      0x16  
    #define X68K_KEY_Z                      0x2A  
    #define X68K_KEY_AT                     0x1B  
    #define X68K_KEY_MINUS                  0x0C  
    #define X68K_KEY_CIRCUMFLEX             0x0D  
    #define X68K_KEY_YEN                    0x0E  
    #define X68K_KEY_BS                     0x0F  
    #define X68K_KEY_TAB                    0x10  
    #define X68K_KEY_OPEN_SQ                0x1C  
    #define X68K_KEY_CLOSE_SQ               0x29  
    #define X68K_KEY_RETURN                 0x1D  
    #define X68K_KEY_SEMI                   0x27  
    #define X68K_KEY_COLON                  0x28  
    #define X68K_KEY_COMMA                  0x31  
    #define X68K_KEY_DOT                    0x32  
    #define X68K_KEY_DIV                    0x33  
    #define X68K_KEY_UNDERLINE              0x34  
    #define X68K_KEY_SPACE                  0x35  
    #define X68K_KEY_HOME                   0x36  
    #define X68K_KEY_ROLLUP                 0x38  
    #define X68K_KEY_ROLLDN                 0x39  
    #define X68K_KEY_UNDO                   0x3A  
    #define X68K_KEY_L_ARROW                0x3B  
    #define X68K_KEY_UP_ARROW               0x3C  
    #define X68K_KEY_R_ARROW                0x3D  
    #define X68K_KEY_DN_ARROW               0x3E  
    #define X68K_KEY_CLR                    0x3F  
    #define X68K_KEY_KP0                    0x4F  
    #define X68K_KEY_KP1                    0x4B  
    #define X68K_KEY_KP2                    0x4C  
    #define X68K_KEY_KP3                    0x4D  
    #define X68K_KEY_KP4                    0x47  
    #define X68K_KEY_KP5                    0x48  
    #define X68K_KEY_KP6                    0x49  
    #define X68K_KEY_KP7                    0x43  
    #define X68K_KEY_KP8                    0x44  
    #define X68K_KEY_KP9                    0x45  
    #define X68K_KEY_KP_DIV                 0x40  
    #define X68K_KEY_KP_TIMES               0x41  
    #define X68K_KEY_KP_MINUS               0x42  
    #define X68K_KEY_KP_PLUS                0x46  
    #define X68K_KEY_KP_EQUAL               0x4A  
    #define X68K_KEY_KP_ENTER               0x4E  
    #define X68K_KEY_KP_COMMA               0x50  
    #define X68K_KEY_KP_DOT                 0x51  
    #define X68K_KEY_SYMBOL                 0x52  
    #define X68K_KEY_HELP                   0x54  
    #define X68K_KEY_CAPS                   0x5D  
    #define X68K_KEY_INS                    0x5E  
    #define X68K_KEY_DEL                    0x37  
    #define X68K_KEY_BREAK                  0x61  
    #define X68K_KEY_COPY                   0x62  
    #define X68K_KEY_SHIFT                  0x70  
    #define X68K_KEY_CTRL                   0x71  
    #define X68K_KEY_XF1                    0x55  
    #define X68K_KEY_XF2                    0x56  
    #define X68K_KEY_XF3                    0x57  
    #define X68K_KEY_XF4                    0x58  
    #define X68K_KEY_XF5                    0x59  
    #define X68K_KEY_REGISTRATION           0x53  
    #define X68K_KEY_KATAKANA               0x5A  
    #define X68K_KEY_ROMAJI                 0x5B  
    #define X68K_KEY_TRANSPOSE              0x5C  
    #define X68K_KEY_HIRAGANA               0x5F  
    #define X68K_KEY_FULLWIDTH              0x60  
    #define X68K_KEY_F1                     0x63  
    #define X68K_KEY_F2                     0x64  
    #define X68K_KEY_F3                     0x65  
    #define X68K_KEY_F4                     0x66  
    #define X68K_KEY_F5                     0x67  
    #define X68K_KEY_F6                     0x68  
    #define X68K_KEY_F7                     0x69  
    #define X68K_KEY_F8                     0x6A  
    #define X68K_KEY_F9                     0x6B  
    #define X68K_KEY_F10                    0x6C  
    #define X68K_KEY_OPT_1                  0x72  
    #define X68K_KEY_OPT_2                  0x73  

    public:
        // Prototypes.
                                        X68K(void);
                                        X68K(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID, const char *fsPath);
                                        X68K(NVS *hdlNVS, HID *hdlHID, const char *fsPath);
                                       ~X68K(void);
        bool                            createKeyMapFile(std::fstream &outFile);
        bool                            storeDataToKeyMapFile(std::fstream &outFile, char *data, int size);
        bool                            storeDataToKeyMapFile(std::fstream & outFile, std::vector<uint32_t>& dataArray);
        bool                            closeAndCommitKeyMapFile(std::fstream &outFile, bool cleanupOnly);
        std::string                     getKeyMapFileName(void) { return(X68KIF_KEYMAP_FILE); };
        void                            getKeyMapHeaders(std::vector<std::string>& headerList);
        void                            getKeyMapTypes(std::vector<std::string>& typeList);
        bool                            getKeyMapSelectList(std::vector<std::pair<std::string, int>>& selectList, std::string option);        
        bool                            getKeyMapData(std::vector<uint32_t>& dataArray, int *row, bool start);

        // Method to return the class version number.
        float version(void)
        {
            return(X68KIF_VERSION);
        }

    protected:

    private:
        // Prototypes.
        IRAM_ATTR void                  pushKeyToQueue(uint32_t key);
        IRAM_ATTR void                  pushHostCmdToQueue(uint8_t cmd);
        IRAM_ATTR static void           x68kInterface( void * pvParameters );
        IRAM_ATTR static void           hidInterface( void * pvParameters );
                  void                  selectOption(uint8_t optionCode);
                  uint32_t              mapKey(uint16_t scanCode);
        bool                            loadKeyMap();
        bool                            saveKeyMap(void);
        void                            init(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID);
        void                            init(NVS *hdlNVS, HID *hdlHID);

        // Structure to encapsulate a single key map from PS/2 to X68K.
        typedef struct {
            uint8_t                     ps2KeyCode;
            uint8_t                     ps2Ctrl;
            uint8_t                     keyboardModel;
            uint8_t                     machine;
            uint8_t                     x68kKey;
            uint8_t                     x68kCtrl;
        } t_keyMapEntry;

        // Structure to encapsulate the entire static keyboard mapping table.
        typedef struct {
            t_keyMapEntry               kme[PS2TBL_X68K_MAXROWS];
        } t_keyMap;
        
        // Structure to maintain the X68000 interface configuration data. This data is persisted through powercycles as needed.
        typedef struct {
            struct {
                uint8_t                 activeKeyboardMap;      // Model of keyboard a keymap entry is applicable to.
                uint8_t                 activeMachineModel;     // Machine model a keymap entry is applicable to.
                bool                    useOnlyPersisted;       // Flag to indicate wether the inbuilt keymap array should be combined with persisted values or the inbuilt array is ignored and only persisted values used.
            } params;
        } t_x68kConfig;
       
        // Configuration data.
        t_x68kConfig                    x68kConfig;

        // Structure to manage the control signals signifying the state of the X68K keyboard.
        typedef struct {
            uint8_t                     keyCtrl;                // Keyboard state flag control.
            bool                        optionSelect;           // Flag to indicate a user requested keyboard configuration option is being selected.
            int                         uartNum;
            int                         uartBufferSize;
            int                         uartQueueSize;

            std::string                 fsPath;                 // Path on the underlying filesystem where storage is mounted and accessible.
            t_keyMapEntry              *kme;                    // Pointer to an array in memory to contain PS2 to X68K mapping values.
            int                         kmeRows;                // Number of rows in the kme table.
            std::string                 keyMapFileName;         // Name of file where extension or replacement key map entries are stored.
            bool                        persistConfig;          // Flag to request saving of the config into NVS storage.
        } t_x68kControl;

        // Transmit buffer queue item.
        typedef struct {
            uint32_t                    keyCode;                // Key data to be sent to X68000.
        } t_xmitQueueMessage;
       
        // Receive buffer queue item.
        typedef struct {
            uint8_t                     hostCmd;  // Keyboard configuration command received from X68000.
        } t_rcvQueueMessage;

        // Thread handles - one per function, ie. HID interface and host target interface.
        TaskHandle_t                    TaskHostIF = NULL;
        TaskHandle_t                    TaskHIDIF  = NULL;

        // Control structure to control interaction and mapping of keys for the host.
        t_x68kControl                   x68kControl;
       
        // Spin lock mutex to hold a coresied to an uninterruptable method. This only works on dual core ESP32's.
        portMUX_TYPE                    x68kMutex;

        // Lookup table to match PS/2 codes to X68K Key and Control Data.
        //
        // Given that the X68K had many variants, with potential differences between them, the mapping table allows for ALL or variant specific entries, the first entry matching is selected.
        //
        // This mapping is for the UK Wyse KB-3926 PS/2 keyboard which is deemed the KEYMAP_STANDARD and all other variants need to add additional mappings below, position sensitive, ie. add non-standard entries before standard entry.
        //
        //const unsigned char PS2toX68K[PS2TBL_X68K_MAXROWS][PS2TBL_X68K_MAXCOLS] =
        //t_keyMapEntry                         PS2toX68K[PS2TBL_X68K_MAXROWS] = 
        t_keyMap                        PS2toX68K = {
        {
            //PS2 Code           PS2 Ctrl (Flags to Match)                                    Keyboard Model                        Machine                   X68K Data                X68K Ctrl (Flags to Set).
            // Function keys
            { PS2_KEY_F1,        PS2CTRL_FUNC | PS2CTRL_CTRL | PS2CTRL_R_CTRL,                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_HIRAGANA,       X68K_CTRL_NONE,                          },   // R_CTRL + F1  = Hiragana
            { PS2_KEY_F2,        PS2CTRL_FUNC | PS2CTRL_CTRL | PS2CTRL_R_CTRL,                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_FULLWIDTH,      X68K_CTRL_NONE,                          },   // R_CTRL + F2  = Full Width
            { PS2_KEY_F3,        PS2CTRL_FUNC | PS2CTRL_CTRL | PS2CTRL_R_CTRL,                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KATAKANA,       X68K_CTRL_NONE,                          },   // R_CTRL + F3  = Katakana
            { PS2_KEY_F4,        PS2CTRL_FUNC | PS2CTRL_CTRL | PS2CTRL_R_CTRL,                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_ROMAJI,         X68K_CTRL_NONE,                          },   // R_CTRL + F4  = Romaji
            { PS2_KEY_F5,        PS2CTRL_FUNC | PS2CTRL_CTRL | PS2CTRL_R_CTRL,                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_TRANSPOSE,      X68K_CTRL_NONE,                          },   // R_CTRL + F5  = Tranpose
            { PS2_KEY_F6,        PS2CTRL_FUNC | PS2CTRL_CTRL | PS2CTRL_R_CTRL,                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_SYMBOL,         X68K_CTRL_NONE,                          },   // R_CTRL + F6  = Symbol
            { PS2_KEY_F7,        PS2CTRL_FUNC | PS2CTRL_CTRL | PS2CTRL_R_CTRL,                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_REGISTRATION,   X68K_CTRL_NONE,                          },   // R_CTRL + F7  = Registration - maybe a poor translation, needs better one!
            { PS2_KEY_F9,        PS2CTRL_FUNC | PS2CTRL_CTRL | PS2CTRL_R_CTRL,                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_COPY,           X68K_CTRL_NONE,                          },   // R_CTRL + F9  = Copy
            { PS2_KEY_F10,       PS2CTRL_FUNC | PS2CTRL_CTRL | PS2CTRL_R_CTRL,                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_HELP,           X68K_CTRL_NONE,                          },   // R_CTRL + F10 = Help
            { PS2_KEY_F1,        PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_F1,             X68K_CTRL_NONE,                          },   // F1
            { PS2_KEY_F2,        PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_F2,             X68K_CTRL_NONE,                          },   // F2
            { PS2_KEY_F3,        PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_F3,             X68K_CTRL_NONE,                          },   // F3
            { PS2_KEY_F4,        PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_F4,             X68K_CTRL_NONE,                          },   // F4
            { PS2_KEY_F5,        PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_F5,             X68K_CTRL_NONE,                          },   // F5
            { PS2_KEY_F6,        PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_F6,             X68K_CTRL_NONE,                          },   // F6
            { PS2_KEY_F7,        PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_F7,             X68K_CTRL_NONE,                          },   // F7
            { PS2_KEY_F8,        PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_F8,             X68K_CTRL_NONE,                          },   // F8
            { PS2_KEY_F9,        PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_F9,             X68K_CTRL_NONE,                          },   // F9
            { PS2_KEY_F10,       PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_F10,            X68K_CTRL_NONE,                          },   // F10
            { PS2_KEY_F11,       PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_OPT_1,          X68K_CTRL_NONE,                          },   // F11 - OPT.1
            { PS2_KEY_F12,       PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_OPT_2,          X68K_CTRL_NONE,                          },   // F12 - OPT.2
            //PS2 Code           PS2 Ctrl (Flags to Match)                                                                          Machine                   X68K Data                X68K Ctrl (Flags to Set).
            // ALPHA keys, case is maaped in the X68000 via the SHIFT key event or CAPS key.                                  
        	{ PS2_KEY_A,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_A,              X68K_CTRL_NONE,                          },   // A
        	{ PS2_KEY_B,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_B,              X68K_CTRL_NONE,                          },   // B
        	{ PS2_KEY_C,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_C,              X68K_CTRL_NONE,                          },   // C
        	{ PS2_KEY_D,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_D,              X68K_CTRL_NONE,                          },   // D
        	{ PS2_KEY_E,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_E,              X68K_CTRL_NONE,                          },   // E
        	{ PS2_KEY_F,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_F,              X68K_CTRL_NONE,                          },   // F
        	{ PS2_KEY_G,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_G,              X68K_CTRL_NONE,                          },   // G
        	{ PS2_KEY_H,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_H,              X68K_CTRL_NONE,                          },   // H
        	{ PS2_KEY_I,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_I,              X68K_CTRL_NONE,                          },   // I
        	{ PS2_KEY_J,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_J,              X68K_CTRL_NONE,                          },   // J
        	{ PS2_KEY_K,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_K,              X68K_CTRL_NONE,                          },   // K
        	{ PS2_KEY_L,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_L,              X68K_CTRL_NONE,                          },   // L
        	{ PS2_KEY_M,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_M,              X68K_CTRL_NONE,                          },   // M
        	{ PS2_KEY_N,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_N,              X68K_CTRL_NONE,                          },   // N
        	{ PS2_KEY_O,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_O,              X68K_CTRL_NONE,                          },   // O
        	{ PS2_KEY_P,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_P,              X68K_CTRL_NONE,                          },   // P
        	{ PS2_KEY_Q,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_Q,              X68K_CTRL_NONE,                          },   // Q
        	{ PS2_KEY_R,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_R,              X68K_CTRL_NONE,                          },   // R
        	{ PS2_KEY_S,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_S,              X68K_CTRL_NONE,                          },   // S
        	{ PS2_KEY_T,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_T,              X68K_CTRL_NONE,                          },   // T
        	{ PS2_KEY_U,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_U,              X68K_CTRL_NONE,                          },   // U
        	{ PS2_KEY_V,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_V,              X68K_CTRL_NONE,                          },   // V
        	{ PS2_KEY_W,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_W,              X68K_CTRL_NONE,                          },   // W
        	{ PS2_KEY_X,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_X,              X68K_CTRL_NONE,                          },   // X
        	{ PS2_KEY_Y,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_Y,              X68K_CTRL_NONE,                          },   // Y
        	{ PS2_KEY_Z,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_Z,              X68K_CTRL_NONE,                          },   // Z
            // Numeric keys.
        	{ PS2_KEY_0,         PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_9,              X68K_CTRL_NONE,                          },   // Close Bracket )
        	{ PS2_KEY_0,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_0,              X68K_CTRL_NONE,                          },   // 0
        	{ PS2_KEY_1,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_1,              X68K_CTRL_NONE,                          },   // 1
        	{ PS2_KEY_2,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_2,              X68K_CTRL_NONE,                          },   // 2
        	{ PS2_KEY_3,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_3,              X68K_CTRL_NONE,                          },   // 3
        	{ PS2_KEY_4,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_4,              X68K_CTRL_NONE,                          },   // 4
        	{ PS2_KEY_5,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_5,              X68K_CTRL_NONE,                          },   // 5
        	{ PS2_KEY_6,         PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_CIRCUMFLEX,     X68K_CTRL_RELEASESHIFT,                  },   // Circumflex ^ 
        	{ PS2_KEY_6,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_6,              X68K_CTRL_NONE,                          },   // 6
        	{ PS2_KEY_7,         PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_6,              X68K_CTRL_NONE,                          },   // &
        	{ PS2_KEY_7,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_7,              X68K_CTRL_NONE,                          },   // 7
        	{ PS2_KEY_8,         PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_COLON,          X68K_CTRL_NONE,                          },   // Start *
        	{ PS2_KEY_8,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_8,              X68K_CTRL_NONE,                          },   // 8
        	{ PS2_KEY_9,         PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_8,              X68K_CTRL_NONE,                          },   // Open Bracket (
        	{ PS2_KEY_9,         PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_9,              X68K_CTRL_NONE,                          },   // 9
            //PS2 Code           PS2 Ctrl (Flags to Match)                                                                          Machine                   X68K Data                X68K Ctrl (Flags to Set).
            // Punctuation keys.
            { PS2_KEY_SPACE,     PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_SPACE,          X68K_CTRL_NONE,                          },   // Space
            { PS2_KEY_MINUS,     PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_CIRCUMFLEX,     X68K_CTRL_NONE,                          },   // Upper Bar 
            { PS2_KEY_MINUS,     PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_MINUS,          X68K_CTRL_NONE,                          },   // Minus -
            { PS2_KEY_EQUAL,     PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_SEMI,           X68K_CTRL_SHIFT,                         },   // Plus +
            { PS2_KEY_EQUAL,     PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_MINUS,          X68K_CTRL_SHIFT,                         },   // Equal =
            { PS2_KEY_DOT,       PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_DOT,            X68K_CTRL_NONE,                          },   // Greater Than >
            { PS2_KEY_DOT,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_DOT,            X68K_CTRL_NONE,                          },   // Dot
            { PS2_KEY_DIV,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_DIV,            X68K_CTRL_NONE,                          },   // Divide /
            { PS2_KEY_SEMI,      PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_COLON,          X68K_CTRL_RELEASESHIFT,                  },   // Colon :
            { PS2_KEY_SEMI,      PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_SEMI,           X68K_CTRL_NONE,                          },   // Semi-Colon ;
            { PS2_KEY_OPEN_SQ,   PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_OPEN_SQ,        X68K_CTRL_NONE,                          },   // [
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_CLOSE_SQ,       X68K_CTRL_NONE,                          },   // ]
        	{ PS2_KEY_APOS,      PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_AT,             X68K_CTRL_RELEASESHIFT,                  },   // @
        	{ PS2_KEY_APOS,      PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_7,              X68K_CTRL_SHIFT,                         },   // '
            { PS2_KEY_BACK,      PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_YEN,            X68K_CTRL_NONE,                          },   // Back slash maps to Yen
            { PS2_KEY_BACK,      PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_YEN,            X68K_CTRL_NONE,                          },   // Back slash maps to Yen
            { PS2_KEY_HASH,      PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_3,              X68K_CTRL_SHIFT,                         },   // Hash
            { PS2_KEY_COMMA,     PS2CTRL_SHIFT,                                               KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_COMMA,          X68K_CTRL_NONE,                          },   // Less Than <
            { PS2_KEY_COMMA,     PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_COMMA,          X68K_CTRL_NONE,                          },   // Comma ,
            { PS2_KEY_BTICK,     PS2CTRL_FUNC | PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_UNDERLINE,      X68K_CTRL_SHIFT,                         },   // Underline
            { PS2_KEY_BTICK,     PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_AT,             X68K_CTRL_SHIFT,                         },   // Back Tick `
            // Control keys.
            { PS2_KEY_TAB,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_TAB,            X68K_CTRL_NONE,                          },   // TAB
            { PS2_KEY_BS,        PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_BS,             X68K_CTRL_NONE,                          },   // Backspace
            { PS2_KEY_ESC,       PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_ESC,            X68K_CTRL_NONE,                          },   // ESCape
            { PS2_KEY_INSERT,    PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_INS,            X68K_CTRL_NONE,                          },   // INSERT
            { PS2_KEY_HOME,      PS2CTRL_FUNC | PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_CLR,            X68K_CTRL_NONE,                          },   // CLR
            { PS2_KEY_HOME,      PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_HOME,           X68K_CTRL_NONE,                          },   // HOME
            { PS2_KEY_DELETE,    PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_DEL,            X68K_CTRL_NONE,                          },   // DELETE
            { PS2_KEY_UP_ARROW,  PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_UP_ARROW,       X68K_CTRL_NONE,                          },   // Up Arrow
            { PS2_KEY_L_ARROW,   PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_L_ARROW,        X68K_CTRL_NONE,                          },   // Left Arrow
            { PS2_KEY_DN_ARROW,  PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_DN_ARROW,       X68K_CTRL_NONE,                          },   // Down Arrow
            { PS2_KEY_R_ARROW,   PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_R_ARROW,        X68K_CTRL_NONE,                          },   // Right Arrow
            { PS2_KEY_PGUP,      PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_ROLLUP,         X68K_CTRL_NONE,                          },   // Roll Up.
            { PS2_KEY_PGDN,      PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_ROLLDN,         X68K_CTRL_NONE,                          },   // Roll Down
            { PS2_KEY_SCROLL,    PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 ' ',                     X68K_CTRL_NONE,                          },   // Not assigned.
            { PS2_KEY_ENTER,     PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_RETURN,         X68K_CTRL_NONE,                          },   // Not assigned.
            { PS2_KEY_CAPS,      PS2CTRL_CAPS,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_CAPS,           X68K_CTRL_NONE,                          },   // CAPS
            { PS2_KEY_END,       PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_UNDO,           X68K_CTRL_NONE,                          },   // UNDO
            // Keypad.
            { PS2_KEY_KP0,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP0,            X68K_CTRL_NONE,                          },   // Keypad 0
            { PS2_KEY_KP1,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP1,            X68K_CTRL_NONE,                          },   // Keypad 1
            { PS2_KEY_KP2,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP2,            X68K_CTRL_NONE,                          },   // Keypad 2
            { PS2_KEY_KP3,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP3,            X68K_CTRL_NONE,                          },   // Keypad 3
            { PS2_KEY_KP4,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP4,            X68K_CTRL_NONE,                          },   // Keypad 4
            { PS2_KEY_KP5,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP5,            X68K_CTRL_NONE,                          },   // Keypad 5
            { PS2_KEY_KP6,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP6,            X68K_CTRL_NONE,                          },   // Keypad 6
            { PS2_KEY_KP7,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP7,            X68K_CTRL_NONE,                          },   // Keypad 7
            { PS2_KEY_KP8,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP8,            X68K_CTRL_NONE,                          },   // Keypad 8
            { PS2_KEY_KP9,       PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP9,            X68K_CTRL_NONE,                          },   // Keypad 9
            { PS2_KEY_KP_COMMA,  PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP_COMMA,       X68K_CTRL_NONE,                          },   // Keypad Comma , 
            { PS2_KEY_KP_DOT,    PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP_DOT,         X68K_CTRL_NONE,                          },   // Keypad Full stop . 
            { PS2_KEY_KP_PLUS,   PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP_PLUS,        X68K_CTRL_NONE,                          },   // Keypad Plus + 
            { PS2_KEY_KP_MINUS,  PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP_MINUS,       X68K_CTRL_NONE,                          },   // Keypad Minus - 
            { PS2_KEY_KP_TIMES,  PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP_TIMES,       X68K_CTRL_NONE,                          },   // Keypad Times * 
            { PS2_KEY_KP_DIV,    PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP_DIV,         X68K_CTRL_NONE,                          },   // Keypad Divide /
            { PS2_KEY_KP_EQUAL,  PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_MINUS,          X68K_CTRL_SHIFT,                         },   // Keypad Equal =
            { PS2_KEY_KP_ENTER,  PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP_ENTER,       X68K_CTRL_NONE,                          },   // Keypad Ebter /
            { PS2_KEY_KP_ENTER,  PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_KP_EQUAL,       X68K_CTRL_NONE,                          },   // Keypad Ebter /
            //PS2 Code           PS2 Ctrl (Flags to Match)                                                                          Machine                   X68K Data                X68K Ctrl (Flags to Set).
            // Special keys.
            { PS2_KEY_PRTSCR,    PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 0x00,                    X68K_CTRL_NONE,                          },   // 
            { PS2_KEY_PAUSE,     PS2CTRL_FUNC | PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_BREAK,          X68K_CTRL_RELEASESHIFT,                  },   // BREAK KEY
            { PS2_KEY_L_GUI,     PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_XF1,            X68K_CTRL_NONE,                          },   // XF1
            { PS2_KEY_L_ALT,     PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_XF2,            X68K_CTRL_NONE,                          },   // XF2
            { PS2_KEY_R_ALT,     PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_XF3,            X68K_CTRL_NONE,                          },   // XF3
            { PS2_KEY_R_GUI,     PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_XF4,            X68K_CTRL_NONE,                          },   // XF4
            { PS2_KEY_MENU,      PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_XF5,            X68K_CTRL_NONE,                          },   // XF5
            // Modifiers are last, only being selected if an earlier match isnt made.
            { PS2_KEY_L_SHIFT,   PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_SHIFT,          X68K_CTRL_NONE,                          },   // 
            { PS2_KEY_R_SHIFT,   PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_SHIFT,          X68K_CTRL_NONE,                          },   // 
            { PS2_KEY_L_CTRL,    PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_CTRL,           X68K_CTRL_NONE,                          },   // Map to Control
            { PS2_KEY_R_CTRL,    PS2CTRL_FUNC,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_CTRL,           X68K_CTRL_NONE,                          },   // Map to Control
            { 0,                 PS2CTRL_NONE,                                                KEYMAP_STANDARD,                      X68K_ALL,                 X68K_KEY_NULL,           X68K_CTRL_NONE,                          },   // 
        }};
};

#endif // X68K_H
