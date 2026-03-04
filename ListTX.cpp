// ListTX - Reads MSH files and outputs required textures.
// Landon Hull aka Calrissian97
// 03/02/26
// Windows Dependencies: Microsoft Visual C++ 2015–2022 Redist
// Linux Dependencies: glibc >= 2.39

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <cstring>
#include <algorithm>

using namespace std;

// Outputs usage when a hero is needed.
static void printUsage(const char* programName)
{
    cout << " Usage: " << filesystem::path(programName).filename().string() << " [options] <files_or_directories>...\n\n"
         << " Options:\n"
         << "  -h, /h, --help, help  Show this help message.\n\n"
         << " Description:\n"
         << "  Scans provided path(s) for .msh files and outputs the required textures listed in them.\n";
}

// Reads MSH binary for MATL chunk and child TXyD chunks and returns unordered set of string texture names.
static void processMshFile(const filesystem::path& path, unordered_set<string>& textures)
{
    // Open file as binary.
    ifstream file(path, ios::binary);
    if (!file.is_open())
    {
        // Output error and problem file.
        cerr << " Error: Unable to open file: " << path.string() << '\n';
        return;
    }
    
    // Skip first 16 bytes (At least HEDR and MSH2 chunks are guaranteed).
    file.seekg(16);

    // 4-byte chunk to read from file.
    char chunk[4];

    // Whether the MATL chunk was found.
    bool matlFound = false;

    // Iterate in 4 byte chunks until "MATL" is located.
    while (file.read(chunk, 4))
    {
        // Compare read chunk to "MATL".
        if (memcmp(chunk, "MATL", 4) == 0)
        {
            // Flag as found, stop reading.
            matlFound = true;
            break;
        }
    }

    // Output error if MATL not found.
    if (!matlFound)
    {
        cerr << " Error: MATL chunk not found in file: " << path.string() << '\n';
        file.close();
        return;
    }

    // Skip 4 bytes (MATL size).
    file.seekg(4, ios::cur);

    // Read count of MATD chunks.
    uint_fast32_t matdCount = 0;
    file.read(reinterpret_cast<char*>(&matdCount), 4);
    const uint_fast8_t MAX_MATERIALS = 255;

    // Defensive check for too many materials/invalid read.
    if (matdCount > MAX_MATERIALS)
    {
        cerr << " Error: Too many MATD chunks in file: " << path.string() << '\n';
        file.close();
        return;
    }

    // Read each MATD chunk.
    for (uint_fast32_t matd = 0; matd < matdCount; ++matd)
    {
        // Skip 4 bytes (MATD header).
        file.seekg(4, ios::cur);

        // Read MATD size.
        uint_fast32_t matdSize = 0;
        file.read(reinterpret_cast<char*>(&matdSize), 4);
        const uint_fast16_t MAX_MATD_SIZE = 2048;

        // Defensive check for invalid read.
        if (matdSize > MAX_MATD_SIZE)
        {
            cerr << " Error: Invalid MATD size in file: " << path.string() << '\n';
            file.close();
            return;
        }

        // Get current position in MATD chunk.
        streamoff matdEnd = file.tellg();

        // Defensive check that streampos was valid.
        if (matdEnd < 0)
        {
            cerr << " Error: Invalid stream position in file: " << path.string() << '\n';
            file.close();
            return;
        }

        // Record end position of MATD chunk.
        matdEnd += matdSize;

        // Skip NAME header.
        file.seekg(4, ios::cur);

        // Read NAME size.
        uint_fast32_t nameSize = 0;
        file.read(reinterpret_cast<char*>(&nameSize), 4);
        const uint16_t MAX_NAME_SIZE = 256;

        // Defensive check for invalid read.
        if (nameSize > MAX_NAME_SIZE)
        {
            cerr << " Error: Invalid NAME chunk size in file: " << path.string() << '\n';
            file.close();
            return;
        }
        
        // Skip MATD->NAME, DATA, and ATRB chunks (size is always fixed at 60 for DATA, 12 for ATRB).
        file.seekg(nameSize + 72, ios::cur);

        // Read TXyD chunks until MATD chunk end Max 4 (TX0D, TX1D, TX2D, TX3D), min 0.
        while (true)
        {
            // Defensive check that stream position was valid.
            std::streamoff pos = file.tellg();
            if (pos < 0 || pos >= matdEnd)
               {
                    break;
               }

            // Skip chunk header (TXyD).
            file.seekg(4, ios::cur);

            // Read chunk size (texture name size).
            uint32_t chunkSize = 0;
            file.read(reinterpret_cast<char*>(&chunkSize), 4);

            // Defensive check for invalid read.
            if (chunkSize > MAX_NAME_SIZE)
            {
                cerr << " Error: Invalid NAME chunk size in file: " << path.string() << '\n';
                file.close();
                return;
            }

            // Read texture name.
            string textureName;
            textureName.resize(chunkSize);
            file.read(&textureName[0], chunkSize);

            // Skip empty texture names (materials without an assigned texture).
            if (textureName.empty() || textureName == ".tga" || textureName[0] == '\0')
            {
                continue;
			}

            // Insert texture name into textures.
            textures.insert(textureName);
        }
    }

    file.close();
    return;
}

// Takes args and outputs textures requried from the MSH(s).
int main(int argc, char* argv[])
{
    // If no arguments given, prompt user for action.
    string inputPath;
    if (argc < 2)
    {
        // Prompt user for input path.
        cout << " Enter a path to a .msh file or a containing directory: " << flush;
        getline(cin, inputPath);

        // Strip quotes from input path.
        if (inputPath.size() >= 2 && inputPath.front() == '"' && inputPath.back() == '"')
        {
            inputPath = inputPath.substr(1, inputPath.size() - 2);
        }

        // Convert to lowercase for checking exit and help commands.
        string lowerInput = inputPath;
        for (auto& c : lowerInput) c = ::tolower(c);

        // Exit immediately.
        if (lowerInput == "q" || lowerInput == "quit" || lowerInput == "exit")
            return 0;

        // Print help then exit.
        if (lowerInput == "help" || inputPath.empty())
        {
            printUsage(argv[0]);
            cout << " Press Enter to exit..." << flush;
            cin.get();
            return 0;
        }
    }

    // Check for help flags first.
    for (int i = 1; i < argc; ++i)
    {
        string lowerArg = argv[i];
        transform(lowerArg.begin(), lowerArg.end(), lowerArg.begin(), ::tolower);
        if (lowerArg == "-h" || lowerArg == "/h" || lowerArg == "--help" || lowerArg == "/help" || lowerArg == "help")
        {
            // If asking for help, provide it.
            printUsage(argv[0]);
            return 0;
        }
    }

    // unordered_set of all textures listed in all input MSH files.
    unordered_set<string> allTextures;

    // Process input paths.
    int pathCount = (argc < 2) ? 1 : argc - 1;
    for (int i = 0; i < pathCount; ++i)
    {
        filesystem::path path;
        if (argc < 2)
            path = inputPath;
        else
            path = argv[i + 1];

        // Check path validity.
        if (!filesystem::exists(path))
        {
            cerr << " Error: Path does not exist: " << path.string() << '\n';
            printUsage(argv[0]);
            continue;
        }
        
        // If directory, iterate upon contents.
        if (filesystem::is_directory(path))
        {
            for (const auto& entry : filesystem::directory_iterator(path))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".msh")
                {
                    processMshFile(entry.path(), allTextures);
                }
            }
        }

        // Otherwise, read input file.
        else if (filesystem::is_regular_file(path))
        {
            if (path.extension() == ".msh")
            {
                processMshFile(path, allTextures);
            }
        }
    }

    // Sort texture names alphabetically into a vector.
    vector<string> sortedTextures(allTextures.begin(), allTextures.end());
    sort(sortedTextures.begin(), sortedTextures.end());

    // Output all listed textures from all input MSH files.
    cout << "\n -- Required Textures --\n";
    for (const auto& texture : sortedTextures)
    {       
        // Print texture name.
        cout << " " << texture << '\n';
    }

    // Keep terminal open until user input.
    cout << "\n Press Enter to exit..." << flush;
    cin.get();
    return 0;
}
