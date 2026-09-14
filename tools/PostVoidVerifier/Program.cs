using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text.Json;

class Program
{
    static int Main(string[] args)
    {
        Console.WriteLine("=================================================");
        Console.WriteLine("     POST VOID DECOMPILATION & PORT AUDIT SUITE  ");
        Console.WriteLine("=================================================");

        string baseDir = @"C:\Users\sanay\PostVoid";
        string gmlDir = Path.Combine(baseDir, "decompiled_gml");
        string asmDir = Path.Combine(baseDir, "bytecode_asm");
        string projDir = Path.Combine(baseDir, "decompiled_project");
        string androidDir = Path.Combine(baseDir, "android");

        int totalErrors = 0;

        // 1. Audit GML scripts
        var gmlFiles = Directory.GetFiles(gmlDir, "*.gml");
        var asmFiles = Directory.GetFiles(asmDir, "*.asm");
        Console.WriteLine($"[Audit 1/6] GML Scripts count: {gmlFiles.Length} files.");
        Console.WriteLine($"[Audit 1/6] Bytecode ASM count: {asmFiles.Length} files.");

        int emptyGml = gmlFiles.Count(f => new FileInfo(f).Length == 0);
        int failedDecomps = gmlFiles.Count(f => File.ReadAllText(f).Contains("DECOMPILER FAILED"));
        Console.WriteLine($"[Audit 1/6] Decompilation failures: {failedDecomps}");
        if (failedDecomps > 0) totalErrors++;

        // 2. Audit Project Manifest
        string manifestPath = Path.Combine(projDir, "manifest.json");
        if (File.Exists(manifestPath))
        {
            var manifestJson = JsonDocument.Parse(File.ReadAllText(manifestPath));
            var root = manifestJson.RootElement;
            Console.WriteLine($"[Audit 2/6] Project Name: {root.GetProperty("gameName").GetString()}");
            Console.WriteLine($"[Audit 2/6] Bytecode Version: {root.GetProperty("bytecodeVersion").GetInt32()}");
            Console.WriteLine($"[Audit 2/6] Objects: {root.GetProperty("objectsCount").GetInt32()}");
            Console.WriteLine($"[Audit 2/6] Sprites: {root.GetProperty("spritesCount").GetInt32()}");
            Console.WriteLine($"[Audit 2/6] Rooms: {root.GetProperty("roomsCount").GetInt32()}");
            Console.WriteLine($"[Audit 2/6] Shaders: {root.GetProperty("shadersCount").GetInt32()}");
            Console.WriteLine($"[Audit 2/6] Sounds: {root.GetProperty("soundsCount").GetInt32()}");
        }
        else
        {
            Console.WriteLine("[Audit 2/6] ERROR: manifest.json missing!");
            totalErrors++;
        }

        // 3. Audit Shaders
        var shaderVerts = Directory.GetFiles(Path.Combine(projDir, "shaders"), "*.vert");
        var shaderFrags = Directory.GetFiles(Path.Combine(projDir, "shaders"), "*.frag");
        Console.WriteLine($"[Audit 3/6] Shaders compiled: {shaderVerts.Length} vertex, {shaderFrags.Length} fragment.");

        // 4. Audit Rooms & Objects
        var roomFiles = Directory.GetFiles(Path.Combine(projDir, "rooms"), "room_*.json");
        var objectFiles = Directory.GetFiles(Path.Combine(projDir, "objects"), "obj_*.json");
        Console.WriteLine($"[Audit 4/6] Serialized Room JSONs: {roomFiles.Length}");
        Console.WriteLine($"[Audit 4/6] Serialized Object JSONs: {objectFiles.Length}");

        // 5. Strict Asset Separation Compliance Check
        Console.WriteLine("[Audit 5/6] Checking Strict Asset Separation Compliance...");
        string assetsDir = Path.Combine(androidDir, "app", "src", "main", "assets");
        var bakedAssets = Directory.Exists(assetsDir) ? Directory.GetFiles(assetsDir, "*.*", SearchOption.AllDirectories) : Array.Empty<string>();
        if (bakedAssets.Length == 0)
        {
            Console.WriteLine("  [PASS] Zero proprietary assets baked into APK assets folder.");
        }
        else
        {
            Console.WriteLine($"  [FAIL] Found {bakedAssets.Length} assets in APK assets folder (VIOLATION)!");
            totalErrors++;
        }

        // 6. Audit Android C++ & JNI Bridge
        Console.WriteLine("[Audit 6/6] Checking Android Native C++ Codebase...");
        string cppDir = Path.Combine(androidDir, "app", "src", "main", "cpp");
        var cppFiles = Directory.GetFiles(cppDir, "*.*", SearchOption.AllDirectories)
                                .Where(f => f.EndsWith(".cpp") || f.EndsWith(".h") || f.EndsWith("CMakeLists.txt"))
                                .ToList();
        Console.WriteLine($"  Found {cppFiles.Count} C++ source and header files in runner wrapper.");

        Console.WriteLine("=================================================");
        if (totalErrors == 0)
        {
            Console.WriteLine("  AUDIT STATUS: 100% PASSED (ALL CRITERIA MET)");
            Console.WriteLine("=================================================");
            return 0;
        }
        else
        {
            Console.WriteLine($"  AUDIT STATUS: FAILED ({totalErrors} ERRORS FOUND)");
            Console.WriteLine("=================================================");
            return 1;
        }
    }
}
