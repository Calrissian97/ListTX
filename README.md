<img width="256" height="256" alt="ListTX" src="https://github.com/user-attachments/assets/8e291c3a-0e35-4b3e-af2a-343413f68ff7" />

# ListTX
Scans provided path(s) for .msh files (Star Wars Battlefront 2004/ Star Wars Battlefront 2 2005 model files) and outputs the required textures listed in them.
## Console Usage
ListTX.exe [options] <files_or_directories>
### Options
-h, /h, --help, help | Show this help message.
## Desktop Usage
Simply drag and drop files or containing directories onto the application to scan them for required textures, or open the application and enter an absolute path.
## Requirements
### Windows Binaries
UCRT is bundled via msvc v142, no redists should be necessary for Windows 7+.
### Linux Binaries
glibc ≥ 2.39, should currently work for Ubuntu 24.04+, Fedora 40+, Arch Linux, Debian Testing/Unstable.
