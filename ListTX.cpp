// ListTX - Reads MSH files and outputs required textures.
// Landon Hull aka Calrissian97
// 03/02/26

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <filesystem>
#ifndef _WIN32
#include <cstring>
#endif

using namespace std;

// Outputs usage when a hero is needed.
void printUsage(const char* programName)
{
    cout << " Usage: " << filesystem::path(programName).filename().string() << " [options] <files_or_directories>...\n\n"
         << " Options:\n"
         << "  -h, /h, --help, help  Show this help message.\n\n"
         << " Description:\n"
         << "  Scans provided path(s) for .msh files and outputs the required textures listed in them.\n";
}

// Reads MSH binary for MATL chunk and child TXyD chunks and returns unordered set of string texture names.
unordered_set<string> processMshFile(const filesystem::path& path)
{
    // List of textures used by all materials in this MSH file.
    unordered_set<string> mshTextures;

    // Open file as binary.
    ifstream file(path, ios::binary);
    if (!file.is_open())
    {
        // Output error and problem file.
        cerr << " Error: Unable to open file: " << path.string() << endl;
        return mshTextures;
    }
    
    // Skip first 16 bytes (At least HEDR and MSH2 chunks are guaranteed).
    file.seekg(16);

    // 4-byte chunk to read.
    char chunk[4];
    // Position of MATL.
    int matlPosition = 0;

    // Iterate in 4 byte chunks until "MATL" is located.
    while (file.read(chunk, 4))
    {
        if (memcmp(chunk, "MATL", 4) == 0)
        {
            // Record starting position of MATL to verify MATL was found.
            matlPosition = static_cast<int>(file.tellg()) - 4;
            break;
        }
    }

    // Output error if MATL not found.
    if (matlPosition == 0)
    {
        cerr << " Error: MATL chunk not found in file: " << path.string() << endl;
        file.close();
        return mshTextures;
    }

    // Skip 4 bytes (MATL size).
    file.seekg(4, ios::cur);

    // Read count of MATD chunks.
    uint32_t matdCount = 0;
    file.read(reinterpret_cast<char*>(&matdCount), 4);

    // Read each MATD chunk
    for (uint_fast8_t matd = 0; matd < matdCount; ++matd)
    {
        // Read next header.
        file.read(chunk, 4);

        // Output error if next header isn't MATD.
        if (memcmp(chunk, "MATD", 4) != 0)
        {
            cerr << " Error: Child MATD chunk not found in file: " << path.string() << endl;
            file.close();
            return mshTextures;
        }

        // Read MATD size.
        uint32_t matdSize = 0;
        file.read(reinterpret_cast<char*>(&matdSize), 4);

        // Record MATD chunk end.
        uint_fast32_t matdEnd = static_cast<int>(file.tellg()) + matdSize;

        // Skip NAME header.
        file.seekg(4, ios::cur);

        // Read NAME size.
        uint32_t nameSize = 0;
        file.read(reinterpret_cast<char*>(&nameSize), 4);
        
        // Skip MATD->NAME, DATA, and ATRB chunks (size is always fixed at 60 for DATA, 12 for ATRB).
        file.seekg(nameSize + 72, ios::cur);

        // Read TXyD chunks until MATD chunk end Max 4 (TX0D, TX1D, TX2D, TX3D), min 0.
        while (static_cast<uint_fast32_t>(file.tellg()) < matdEnd)
        {
            // Skip chunk header (TXyD).
            file.seekg(4, ios::cur);

            // Read chunk size (also texture name size).
            uint32_t chunkSize = 0;
            file.read(reinterpret_cast<char*>(&chunkSize), 4);

            // Read texture name.
            string textureName;
            textureName.resize(chunkSize);
            file.read(&textureName[0], chunkSize);

            if (textureName.empty() || textureName == ".tga" || textureName[0] == '\0')
            {
                // Skip empty texture names (materials without a texture).
                continue;
			}

            // Insert texture name into mshTextures.
            mshTextures.insert(textureName);
        }
    }

    // Explicitly close file to be safe.
    file.close();

    // Return textures listed in this MSH.
    return mshTextures;
}

// Takes args and outputs textures requried from the MSH(s).
int main(int argc, char* argv[])
{
    // If no arguments given, prompt user for action.
    static string inputPath;
    if (argc < 2)
    {
        // Prompt user for input path.
        cout << " Enter a path to a .msh file or a containing directory: ";
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
            cout << "\n Press Enter to exit...";
            cin.get();
            return 0;
        }
    }

    // Check for help flags first.
    for (int i = 1; i < argc; ++i)
    {
        string lowerArg = argv[i];
        transform(lowerArg.begin(), lowerArg.end(), lowerArg.begin(), ::tolower);
        if (lowerArg == "-h" || lowerArg == "/h" || lowerArg == "--help" || lowerArg == "help")
        {
            // If asking for help, provide it.
            printUsage(argv[0]);
            return 0;
        }
    }

    // Static set of all textures listed in all input MSH files.
    static unordered_set<string> allTextures;

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
            cerr << " Error: Path does not exist: " << argv[i] << endl;
            continue;
        }
        
        // If directory, iterate upon contents.
        if (filesystem::is_directory(path))
        {
            for (const auto& entry : filesystem::directory_iterator(path))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".msh")
                {
                    unordered_set<string> mshTextures = processMshFile(entry.path());
                    allTextures.insert(mshTextures.begin(), mshTextures.end());
                }
            }
        }

        // Otherwise, read input file.
        else if (filesystem::is_regular_file(path))
        {
            if (path.extension() == ".msh")
            {
                    unordered_set<string> mshTextures = processMshFile(path);
                    allTextures.insert(mshTextures.begin(), mshTextures.end());
            }
        }
    }

    // Output all listed textures from all input MSH files.
    cout << "\n -- Required Textures --" << endl;
    for (const string texture : allTextures)
    {
        // Skip empty strings (materials without a texture).
        if (texture.empty() || texture.length() == 0)
            continue;
            
        // Print texture name.
        cout << " " << texture << endl;
    }

    // Keep terminal open until user input.
    cout << "\n Press Enter to exit...";
    cin.get();

    // Return 0 on exit
    return 0;
}
