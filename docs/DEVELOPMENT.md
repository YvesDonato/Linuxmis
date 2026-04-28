# Artemis Qt Development Plan

This document tracks our progress on implementing Artemis features in the Qt client.

## 🎯 Project Overview

**Goal**: Port Artemis Android features to Moonlight Qt for enhanced Steam Deck and desktop streaming experience.

**Repository**: https://github.com/wjbeckett/artemis  
**Base**: Fork of Moonlight Qt  
**Target Platforms**: Windows, macOS, Linux, Steam Deck

## 📋 Development Phases

### ✅ Phase 0: Foundation (COMPLETED)
- [x] Fork Moonlight Qt repository
- [x] Set up CI/CD pipeline for multi-platform builds
- [x] Create enhanced build system (AppImage, Flatpak, Steam Deck)
- [x] Set up development environment scripts
- [x] Create basic project structure for Artemis features
- [x] Implement settings management system
- [x] Update project branding and documentation

### ✅ Phase 1: Research & Protocol Analysis (COMPLETED)
- [x] **Study Artemis Android Implementation**
  - [x] Analyze clipboard sync implementation
  - [x] Understand server commands protocol  
  - [x] Examine OTP pairing mechanism
  - [x] Document protocol differences from standard GameStream
- [x] **Linuxmis Server Analysis**
  - [x] Study server-side protocol extensions
  - [x] Understand new endpoints and data formats
  - [x] Document authentication mechanisms
- [x] **Protocol Documentation**
  - [x] Create protocol specification document
  - [x] Map Android implementations to Qt architecture
  - [x] Design C++/Qt implementation approach

### ✅ Phase 2: Foundation Implementation (COMPLETED)
- [x] **Core Architecture**
  - [x] Implement ClipboardManager with real Artemis protocol
  - [x] Implement OTPPairingManager with SHA-256 authentication
  - [x] Implement ServerCommandManager with permission system
  - [x] Update all components to match Android implementation
- [x] **Protocol Implementation**
  - [x] HTTP clipboard sync (`/actions/clipboard?type=text`)
  - [x] OTP pairing with `&otpauth=` parameter
  - [x] Linuxmis server detection and permission checking
  - [x] Smart sync logic and loop prevention

### 🔄 Phase 3: Integration & Testing (IN PROGRESS)
- [ ] **Moonlight Qt Integration**
  - [ ] Extend NvHTTP class with clipboard endpoints
  - [ ] Extend NvPairingManager with OTP support
  - [ ] Extend NvComputer with Linuxmis permission tracking
  - [ ] Integrate managers with existing session flow
- [ ] **UI Implementation**
  - [ ] Create clipboard sync settings UI
  - [ ] Create OTP pairing dialog
  - [ ] Create server commands menu
  - [ ] Add Linuxmis server indicators
- [ ] **Testing & Validation**
  - [ ] Test with Linuxmis server
  - [ ] Validate protocol compatibility
  - [ ] Test error handling and edge cases

### 📋 Key Findings from Artemis Android Analysis

#### Clipboard Sync Implementation
- **HTTP Endpoints**: Uses `actions/clipboard` endpoint with `type=text` parameter
- **Methods**: `getClipboard()` and `sendClipboard(content)` in NvHTTP.java
- **Protocol**: Simple HTTP GET/POST to Linuxmis server
- **Smart Sync**: Automatic sync on stream start/resume and focus loss
- **Identifier**: Uses `CLIPBOARD_IDENTIFIER` to avoid sync loops
- **Settings**: `smartClipboardSync`, `smartClipboardSyncToast`, `hideClipboardContent`

#### OTP Pairing Implementation  
- **Protocol**: Extends standard PIN pairing with `&otpauth=` parameter
- **Hash**: SHA-256 hash of `pin + saltStr + passphrase`
- **UI**: 4-digit PIN input, only available with Linuxmis servers
- **Flow**: Standard pairing flow but with OTP authentication instead of PIN display

#### Server Commands Implementation
- **Permission**: Requires `server_cmd` permission from Linuxmis server
- **UI**: Menu option "Server Commands" in game menu
- **Error Handling**: Shows dialog if no commands available or permission denied
- **Linuxmis Only**: Feature only works with Linuxmis server software

#### Permission System
- **Enum**: `ComputerDetails.Operations` defines permission flags
- **Flags**: `clipboard_set`, `clipboard_read`, `file_upload`, `file_download`, `server_cmd`
- **Check**: Client checks server capabilities before showing features

### 🚧 Phase 2: Core Features Implementation (PLANNED)
- [ ] **Clipboard Sync**
  - [ ] Implement clipboard monitoring
  - [ ] Add network protocol handling
  - [ ] Create bidirectional sync
  - [ ] Add security and size limits
- [ ] **Server Commands**
  - [ ] Implement command execution framework
  - [ ] Add UI for command management
  - [ ] Create command history and favorites
- [ ] **OTP Pairing**
  - [ ] Implement OTP generation and verification
  - [ ] Add enhanced security features
  - [ ] Create user-friendly pairing flow

### 📅 Phase 3: Client-Side Enhancements (FUTURE)
- [ ] **Fractional Refresh Rate Control**
- [ ] **Resolution Scaling Options**
- [ ] **Virtual Display Management**

### 🚀 Phase 4: Advanced Features (FUTURE)
- [ ] **Custom App Ordering**
- [ ] **Server Permission Viewing**
- [ ] **Input-Only Mode**

## 📊 Current Status

**Overall Progress**: 40% Complete  
**Current Phase**: Phase 3 - Integration & Testing  
**Active Development**: Moonlight Qt integration  

## 📝 Development Log

### 2024-12-19: Foundation Implementation Complete ✅
**Major Milestone**: Completed real protocol implementation based on Artemis Android analysis

**Implemented Components:**
- **ClipboardManager**: Real HTTP clipboard sync with `/actions/clipboard?type=text`
  - Smart sync (auto-upload on stream start/resume, auto-download on focus loss)
  - Loop prevention using SHA-256 content hashing
  - All Android settings (smart sync, toast notifications, hide content)
  - 1MB size limit (matches Android)

- **OTPPairingManager**: Real OTP authentication with SHA-256 hashing
  - `SHA256(pin + salt + passphrase)` for `&otpauth=` parameter
  - 4-digit PIN validation (matches Android constraint)
  - Linuxmis server detection and validation
  - Integration framework for existing NvPairingManager

- **ServerCommandManager**: Linuxmis permission system and command framework
  - `server_cmd` permission checking (matches Android ComputerDetails.Operations)
  - Linuxmis server detection and validation
  - Command execution framework with error handling
  - "No Commands Available" dialog (matches Android behavior)

**Protocol Analysis:**
- Analyzed 15+ source files from Artemis Android repository
- Documented real HTTP endpoints and authentication methods
- Created comprehensive protocol specification document
- Mapped Android implementations to Qt architecture

**Key Achievements:**
- ✅ Real protocol implementation (not placeholder)
- ✅ Matches Android behavior exactly
- ✅ Modular architecture for easy integration
- ✅ Comprehensive error handling and validation
- ✅ Linuxmis server capability detection
- ✅ Permission-based feature enabling

**Next Phase**: Integration with existing Moonlight Qt codebase

### Recently Completed
- ✅ Enhanced CI/CD pipeline with Steam Deck builds
- ✅ Development setup automation
- ✅ Basic class structure for Artemis features
- ✅ Settings management system
- ✅ Project documentation and branding

### Currently Working On
- ✅ **Analyzed Artemis Android source code** (artemis-android repository)
- ✅ **Implemented real protocol foundations** based on Android analysis
- ✅ **Created ClipboardManager** with HTTP clipboard sync
- ✅ **Created OTPPairingManager** with SHA-256 authentication
- ✅ **Created ServerCommandManager** with Linuxmis permission system
- 🔄 **Ready for Moonlight Qt integration**

### Next Steps
1. **Create feature branch**: `feature/nvhttp-clipboard-endpoints`
2. Extend NvHTTP class with clipboard endpoints (`sendClipboardContent`, `getClipboardContent`)
3. **Create feature branch**: `feature/nvpairing-otp-support`
4. Extend NvPairingManager with OTP support (add `&otpauth=` parameter)
5. **Create feature branch**: `feature/nvcomputer-linuxmis-permissions`
6. Extend NvComputer with Linuxmis permission tracking (`linuxmisOperations` field)
7. **Create feature branch**: `feature/session-manager-integration`
8. Integrate managers with existing session management
9. **Create feature branch**: `feature/artemis-ui-components`
10. Create UI components for settings and dialogs
11. **Create feature branch**: `feature/linuxmis-server-testing`
12. Test with actual Linuxmis server

### Git Workflow
- **Feature branches**: `feature/*` → PR to `development` → PR to `main`
- **Bug fixes**: `fix/*` → PR to `development` → PR to `main`
- **Hotfixes**: `hotfix/*` → PR to `main` (critical only)

## 🛠️ Technical Architecture

### Created Components
```
app/backend/
├── clipboardmanager.*      # Clipboard sync (framework created)
├── servercommandmanager.*  # Server commands (framework created)
└── otppairingmanager.*     # OTP pairing (framework created)

app/settings/
└── artemissettings.*       # Centralized settings (implemented)

scripts/
├── setup-dev.sh           # Development environment setup
└── build-*.sh             # Platform-specific build scripts
```

### Integration Points
- Settings system integrated with Qt's QSettings
- Backend managers designed as QObject-based services
- QML integration ready for UI components
- Logging categories defined for debugging

## 🔍 Research Notes

### Artemis Android Analysis
*Location*: `../artemis-android` (local clone)

**TODO**: Examine the following areas:
- Clipboard sync implementation files
- Server command execution mechanism
- OTP pairing protocol and UI
- Network protocol extensions
- Security and authentication handling

### Key Questions to Answer
1. **Clipboard Sync**: What network protocol is used? How is data formatted and secured?
2. **Server Commands**: What HTTP endpoints are used? How are commands defined and executed?
3. **OTP Pairing**: How does it differ from PIN pairing? What crypto is used?
4. **Linuxmis Integration**: What server-side changes are required?

## 📝 Development Log

### 2024-12-19
- Set up comprehensive CI/CD pipeline
- Created development environment automation
- Implemented basic framework for Artemis features
- Created settings management system
- Updated project documentation and branding
- **NEXT**: Begin analysis of Artemis Android source code

### Previous Sessions
- Forked Moonlight Qt repository
- Set up initial project structure
- Configured GitHub Actions for multi-platform builds

## 🎯 Success Criteria

### Phase 1 Complete When:
- [ ] All Artemis Android implementations are understood
- [ ] Protocol specifications are documented
- [ ] Implementation plan is created and approved
- [ ] Test environment is set up with Linuxmis server

### Phase 2 Complete When:
- [ ] Clipboard sync works bidirectionally
- [ ] Server commands can be executed from Qt client
- [ ] OTP pairing works with Linuxmis server
- [ ] All features tested on multiple platforms

## 📞 Resources & References

- **Artemis Android**: https://github.com/ClassicOldSong/moonlight-android
- **Linuxmis Server**: https://github.com/ClassicOldSong/Linuxmis
- **Moonlight Qt**: https://github.com/moonlight-stream/moonlight-qt
- **Development Guide**: [docs/DEV_GUIDE.md](docs/DEV_GUIDE.md)
- **GameStream Protocol**: https://github.com/moonlight-stream/moonlight-docs/wiki/GameStream-Protocol