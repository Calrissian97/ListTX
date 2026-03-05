// ListTX - Reads MSH, FX, Option, and ODF files and outputs required textures.
// Landon Hull aka Calrissian97
// 03/05/26
// Linux Dependencies: glibc >= 2.39

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <algorithm>

using namespace std;

// Parse option flags struct.
struct ScanOptions {
    bool scanMSH = false;
    bool scanODF = false;
    bool scanFX  = false;
    bool scanSKY = false;
};

// Textures associated with a single file.
struct FileTextureResult {
    string filename;                 // File name only, e.g. "example.msh".
    string extension;                // File extension, e.g. ".msh".
    unordered_set<string> textures;  // Unique textures required by this file.
};

// Outputs usage when a hero is needed.
static void printUsage(const char *programName)
{
    cout << " Usage: " << filesystem::path(programName).filename().string() << " [options] <files_or_directories>...\n\n"
         << " Options:\n"
         << "  -h, /h, --help, help  Show this help message.\n"
         << "  -msh, /msh  Scan msh files and msh.option files.\n"
         << "  -odf, /odf  Scan odf files."
         << "  -fx, /fx  Scan world fx or particle fx files.\n"
         << "  -sky, /sky  Scan world sky files.\n\n"
         << " Description:\n"
         << "  Scans provided path(s) for BF 1/2 files and outputs the required textures listed in them.\n"
         << "  By default input .msh, .msh.option, .odf, .fx, and .sky files will be scanned.\n";
}

// Outputs resulting group by file types.
static void printGroup(const string &label, const string &extension, vector<FileTextureResult> &results)
{
    // Check that any group has been scanned.
    bool anyGroup = false;
    for (const auto &fileTextureResult : results)
    {
        if (fileTextureResult.extension == extension)
        {
            anyGroup = true;
            break;
        }
    }
    // If no group has been scanned, simply return.
    if (!anyGroup)
    {
        return;
    }

    // Output filetype label (MSH, ODF, FX, etc.)
    cout << " " << label << "--------------\n";

    // Iterate upon vector of FileTextureResults.
    for (const auto &fileTextureResult : results)
    {
        // If results are appropriate for the selected extension, print that group.
        if (fileTextureResult.extension == extension)
        {
            // Print file name.
            cout << " +" << fileTextureResult.filename << '\n';

            // Sort texture names for that file.
            vector<string> sorted(fileTextureResult.textures.begin(), fileTextureResult.textures.end());
            sort(sorted.begin(), sorted.end());

            // Print sorted texture names.
            for (const auto &t : sorted)
                cout << " |-" << t << '\n';
        }
    }

    cout << '\n';
    return;
}

// Helper function to trim whitespace from strings.
static void trimWhitespace(string &str)
{
    // Find first char position of texture name.
    int_fast32_t firstChar = str.find_first_not_of(" \t\r\n");
    if (firstChar == string::npos)
    {
        str.clear();
        return;
    }
    // Find last char position of texture name.
    int_fast32_t lastChar = str.find_last_not_of(" \t\r\n");
    if (lastChar == string::npos)
    {
        str.clear();
        return;
    }

    // Reduce string to non-whitespace segment.
    str = str.substr(firstChar, lastChar - firstChar + 1);
}

// Reads SKY text file for required textures and returns unordered set of string texture names.
static void processSkyFile(const filesystem::path &path, unordered_set<string> &textures)
{
    // Open input file as reading stream
    ifstream file(path);

    // If file cannot be opened, note error and return.
    if (!file.is_open())
    {
        cerr << " Error: Unable to open file: " << path.string() << '\n';
        return;
    }

    // Token flags that indicate a texture is required.
    const string textureIndicators[5] = {
        "texture(\"", "startexture(\"", "envtexture(\"", "detailtexture(\"", "terrainbumpdetail(\""
    };

    // Read file line by line to EOF.
    string line;
    while (getline(file, line))
    {
        // Convert to lowercase for matching.
        string lineLower = line;
        for (char& c : lineLower)
        {
            c = static_cast<char>(tolower(c));
        }

        // Scan for any indicator in this line.
        for (const string &key : textureIndicators)
        {
            int_fast32_t textureNameStart = lineLower.find(key);

            // If not found, skip to next line.
            if (textureNameStart == string::npos)
            {
                continue;
            }

            // Move to first character after the opening quote.
            textureNameStart += key.size();

            // Find the closing quote.
            int_fast32_t textureNameEnd = line.find('"', textureNameStart);

            // If not found, skip to next line.
            if (textureNameEnd == string::npos)
            {
                continue; // malformed, line, skip to next.
            }

            // Extract the texture name from the line.
            string textureName = line.substr(textureNameStart, textureNameEnd - textureNameStart);

            // Trim whitespace from start or end of name.
            trimWhitespace(textureName);

            // Append .tga extension and add to textures.
            if (!textureName.empty())
            {
                // If .tga extension already present, don't append.
                if (textureName.find(".tga") == string::npos)
                {
                    textureName.append(".tga");
                }
                textures.insert(textureName);
            }
        }
    }

    file.close();
    return;
}

// Reads ODF text file for required textures and returns unordered set of string texture names.
static void processOdfFile(const filesystem::path &path, unordered_set<string> &textures)
{
    // Open input file as reading stream.
    ifstream file(path);

    // If file cannot be opened, note error and return.
    if (!file.is_open())
    {
        cerr << " Error: Unable to open file: " << path.string() << '\n';
        return;
    }

    // Token flags that indicate a texture is required.
    const string textureIndicators[15] = {
        "texture", "icontexture", "healthtexture", "maptexture", "reticuletexture", "scopetexture",
        "lightsabertexture", "lightsaberglowtexture", "wheeltexture", "overridetexture", "overridetexture2", "projectedtexture",
        "beamtexture", "beamglowtexture", "lasertexture"
    };

    // Read file line by line until EOF.
    string line;
    while (getline(file, line))
    {
        // Find '=' separating parameter name and value.
        int_fast32_t equalsSign = line.find('=');

        // If not found, skip to next line.
        if (equalsSign == string::npos)
        {
            continue;
        }

        // Extract and trim parameter name of whitespace.
        string OdfParameter = line.substr(0, equalsSign);
        trimWhitespace(OdfParameter);

        // Lowercase for comparison.
        string lineLower = OdfParameter;
        for (char &c : lineLower)
        {
            c = static_cast<char>(tolower(c));
        }

        // Check if this parameter is one of the texture indicators
        bool isTextureIndicator = false;
        for (const string &key : textureIndicators)
        {
            if (lineLower == key)
            {
                isTextureIndicator = true;
                break;
            }
        }
        // If not found, skip to next line.
        if (!isTextureIndicator)
        {
            continue;
        }

        // Extract the value part (after '=')
        string OdfValue = line.substr(equalsSign + 1);
        trimWhitespace(OdfValue);

        // Find first quote in value segment.
        int_fast32_t valueStart = OdfValue.find('"');

        // If not found, skip to next line.
        if (valueStart == string::npos)
        {
            continue;
        }

        // Find last quote in value segment.
        int_fast32_t valueEnd = OdfValue.find('"', valueStart + 1);

        // If not found, skip to next line.
        if (valueEnd == string::npos)
        {
            continue;
        }

        // Extract texture name.
        string textureName = OdfValue.substr(valueStart + 1, valueEnd - (valueStart + 1));
        trimWhitespace(textureName);

        // Append .tga extension and add to textures.
        if (!textureName.empty())
        {
            textureName.append(".tga");
            textures.insert(textureName);
        }
    }

    file.close();
    return;
}

// Reads msh.option text file for required textures and returns unordered set of string texture names.
static void processOptionFile(const filesystem::path &path, unordered_set<string> &textures)
{
    // Open input file as reading stream.
    ifstream file(path);

    // If file cannot be opened, note error and return.
    if (!file.is_open())
    {
        cerr << " Error: Unable to open file: " << path.string() << '\n';
        return;
    }

    // Flag to indicate "-bump" parameter was found.
    bool readingBump = false;

    // Read file line by line until EOF.
    string line;
    while (getline(file, line))
    {
        // Tokenize by whitespace.
        istringstream iss(line);
        string token;

        // Stream each token until EOL.
        while (iss >> token)
        {
            // Lowercase copy for comparisons/
            string tokenLower = token;
            for (char &c : tokenLower)
                c = static_cast<char>(tolower(c));

            // Detect new parameter (tokens starting with '-').
            if (!token.empty() && token[0] == '-')
            {
                // Check if this parameter is -bump (case-insensitive).
                if (tokenLower == "-bump")
                {
                    // Flag bump parameter as found.
                    readingBump = true;
                    continue;
                }
                else
                {
                    // Any other parameter ends "bump-reading mode".
                    readingBump = false;
                    continue;
                }
            }

            // If we are in "bump-reading mode", this token is a texture name.
            if (readingBump)
            {
                // Trim whitespace from token start, end.
                trimWhitespace(token);

                // Append "_bump" and ".tga" extension and add to textures.
                if (!token.empty())
                {
                    token.append("_bump.tga");
                    textures.insert(token);
                }
            }
        }
    }

    file.close();
    return;
}

// Reads .fx text file for required textures and returns unordered set of string texture names.
static void processFxFile(const filesystem::path &path, unordered_set<string> &textures)
{
    // Open input file as reading stream.
    ifstream file(path);

    // If file cannot be opened, note error and return.
    if (!file.is_open())
    {
        cerr << " Error: Unable to open file: " << path.string() << '\n';
        return;
    }

    // Token flags that indicate a texture is required.
    const string textureIndicators[5] = {
        "texture(\"", "maintexture(\"", "normalmaptextures(\"", "bumpmaptextures(\"", "specularmasktextures(\""
    };

    // Read file line by line until EOF.
    string line;
    while (getline(file, line))
    {
        // Lowercase copy for indicator matching.
        string lineLower = line;
        for (char &c : lineLower)
        {
            c = static_cast<char>(tolower(c));
        }

        // Check each indicator.
        for (const string &key : textureIndicators)
        {
            int_fast32_t indicatorStart = lineLower.find(key);

            // If not found, skip to next line.
            if (indicatorStart == string::npos)
            {
                continue;
            }

            // Move to first char after the opening quote.
            int_fast32_t textureNameStart = indicatorStart + key.size();

            // Water textures require special handling (multiple textures specified by suffix).
            if (key == textureIndicators[2] ||
                key == textureIndicators[3] ||
                key == textureIndicators[4])
            {
                // Find end of texture name prefix.
                int_fast32_t textureNameEnd = line.find('"', textureNameStart);

                // If not found, skip to next line.
                if (textureNameEnd == string::npos)
                {
                    continue;
                }

                // Extract texture name prefix and trim white space from start and end.
                string textureNamePrefix = line.substr(textureNameStart, textureNameEnd - textureNameStart);
                trimWhitespace(textureNamePrefix);

                // After prefix, expect: ,<count>,
                int_fast32_t comma1 = line.find(',', textureNameEnd + 1);

                // If not found, skip to next line.
                if (comma1 == string::npos)
                {
                    continue;
                }

                int_fast32_t comma2 = line.find(',', comma1 + 1);

                // If not found, skip to next line.
                if (comma2 == string::npos)
                {
                    continue;
                }

                // Extract count (integer)
                string countStr = line.substr(comma1 + 1, comma2 - (comma1 + 1));
                trimWhitespace(countStr);

                // Count of textures for this type (normalmap, bumpmap, specularmask).
                uint_fast8_t count = 0;
                try {
                    // Attempt extacting integer value.
                    count = stoi(countStr);
                } catch (...) {
                    // Note error and skip to next line if invalid value.
                    cerr << " Warning: Invalid count in water texture line: "
                         << path.string() << '\n';
                    continue;
                }

                // Generate textures prefix_0 ... prefix_(count-1).
                for (int i = 0; i < count; ++i)
                {
                    string textureName = textureNamePrefix + to_string(i) + ".tga";
                    textures.insert(textureName);
                }
                
                // Skip to next line (could be single texture).
                continue;
            }

            // If not a water texture, it's a normal texture line.
            int_fast32_t textureNameEnd = line.find('"', textureNameStart);

            // If not found, skip to next line.
            if (textureNameEnd == string::npos)
            {
                cerr << " Warning: Malformed texture line in file: "
                     << path.string() << " (no closing quote)\n";
                continue;
            }

            // Extract texture name and trim whitespace from start and end.
            string textureName = line.substr(textureNameStart, textureNameEnd - textureNameStart);
            trimWhitespace(textureName);

            // Append .tga extension and add to textures.
            if (!textureName.empty())
            {
                textureName.append(".tga");
                textures.insert(textureName);
            }
        }
    }

    file.close();
}

// Reads MSH binary for MATL chunk and child TXyD chunks and returns unordered set of string texture names.
static void processMshFile(const filesystem::path &path, unordered_set<string> &textures)
{
    // Open input file stream as binary reading stream.
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
    file.read(reinterpret_cast<char *>(&matdCount), 4);
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
        file.read(reinterpret_cast<char *>(&matdSize), 4);
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
        file.read(reinterpret_cast<char *>(&nameSize), 4);
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
            file.read(reinterpret_cast<char *>(&chunkSize), 4);

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

// Processes a file path by calling the related function(s) based on it's extension.
static void processFile(const filesystem::path &filePath, const ScanOptions &options, FileTextureResult &result)
{
    // Record basic file info for later grouping/printing.
    result.filename  = filePath.filename().string();
    result.extension = filePath.extension().string();

    // Record extension and convert to lowercase for comparison.
    string ext = result.extension;
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Scan input msh file.
    if (options.scanMSH && ext == ".msh")
        processMshFile(filePath, result.textures);
    // Scan input msh.option file.
    else if (options.scanMSH && ext == ".option") // .msh.option
        processOptionFile(filePath, result.textures);
    // Scan input odf file.
    else if (options.scanODF && ext == ".odf")
        processOdfFile(filePath, result.textures);
    // Scan input fx file.
    else if (options.scanFX && ext == ".fx")
        processFxFile(filePath, result.textures);
    // Scan input sky file.
    else if (options.scanSKY && ext == ".sky")
        processSkyFile(filePath, result.textures);

    return;
}

// Takes args and outputs textures requried from the BF 1/2 file(s).
int main(int argc, char *argv[])
{
    // If no arguments given, prompt user for action.
    string inputPath;
    if (argc < 2)
    {
        // Prompt user for input path.
        cout << " Enter a path to a file or containing directory: " << flush;
        getline(cin, inputPath);

        // If path is quoted, trim quotes from input.
        if (inputPath.size() >= 2 && inputPath.front() == '"' && inputPath.back() == '"')
        {
            inputPath = inputPath.substr(1, inputPath.size() - 2);
        }

        // Change to lowercase for matching.
        string lowerInput = inputPath;
        for (auto &c : lowerInput)
        {
            c = ::tolower(c);
        }

        // Exit immediately if prompted.
        if (lowerInput == "q" || lowerInput == "quit" || lowerInput == "exit")
        {
            return 0;
        }

        // Provide a hero if help is asked.
        if (lowerInput == "help" || inputPath.empty())
        {
            printUsage(argv[0]);
            cout << " Press Enter to exit..." << flush;
            cin.get();
            return 0;
        }
    }

    // Parse options
    ScanOptions options;

    // Collect input paths into a vector of strings.
    vector<string> paths;

    // Process arguments given.
    for (int i = 1; i < argc; ++i)
    {
        string arg = argv[i];
        string lowerArg = arg;
        transform(lowerArg.begin(), lowerArg.end(), lowerArg.begin(), ::tolower);

        // Provide a hero if help is asked.
        if (lowerArg == "-h" || lowerArg == "/h" || lowerArg == "--help" || lowerArg == "/help" || lowerArg == "help")
        {
            printUsage(argv[0]);
            return 0;
        }
        // If msh is given enable scanning .msh and .msh.option files.
        else if (lowerArg == "-msh" || lowerArg == "/msh")
        {
            options.scanMSH = true;
        }
        // If odf is given enable scanning odf files.
        else if (lowerArg == "-odf" || lowerArg == "/odf")
        {
            options.scanODF = true;
        }
        // If fx is given enable scanning fx files.
        else if (lowerArg == "-fx" || lowerArg == "/fx")
        {
            options.scanFX = true;
        }
        // If sky is given enable scanning sky files.
        else if (lowerArg == "-sky" || lowerArg == "/sky")
        {
            options.scanSKY = true;
        }
        // If not a valid option, treat as a path.
        else
        {
            paths.push_back(arg);
        }
    }

    // If no options were provided, enable all by default.
    if (!options.scanMSH && !options.scanODF && !options.scanFX && !options.scanSKY)
    {
        options.scanMSH = options.scanODF = options.scanFX = options.scanSKY = true;
    }

    // If no paths were provided (argc < 2 case) simple add input path.
    if (argc < 2)
    {
        paths.push_back(inputPath);
    }

    // Per-file texture results.
    vector<FileTextureResult> results;

    // Process paths.
    for (const string &p : paths)
    {
        // Construct path from input string(s).
        filesystem::path path = p;

        // If path is invalid, note error, print help, and continue.
        if (!filesystem::exists(path))
        {
            cerr << " Error: Path does not exist: " << path.string() << '\n';
            printUsage(argv[0]);
            continue;
        }

        // Process each BF 1/2 file in the input directory.
        if (filesystem::is_directory(path))
        {
            for (const auto &entry : filesystem::directory_iterator(path))
            {
                if (entry.is_regular_file())
                {
                    FileTextureResult fileTextureResult;
                    processFile(entry.path(), options, fileTextureResult);
                    if (!fileTextureResult.textures.empty())
                        results.push_back(move(fileTextureResult));
                }
            }
        }
        // Process input file(s).
        else if (filesystem::is_regular_file(path))
        {
            FileTextureResult fileTextureResult;
            processFile(path, options, fileTextureResult);
            if (!fileTextureResult.textures.empty())
                results.push_back(move(fileTextureResult));
        }
    }

    // Output required textures grouped by file type.
    cout << "\n -- Required Textures --\n";
    printGroup("MSH Files", ".msh", results);
    printGroup("Option Files", ".option", results);
    printGroup("ODF Files", ".odf", results);
    printGroup("FX Files", ".fx", results);
    printGroup("SKY Files", ".sky", results);

    // Prompt user for input before exiting.
    cout << " Press Enter to exit..." << flush;
    cin.get();
    return 0;
}
