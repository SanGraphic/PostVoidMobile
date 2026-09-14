using System;
using System.Diagnostics;
using System.IO;
using UndertaleModLib;
using UndertaleModLib.Models;

class Program {
    static byte[] ConvertWavToOgg(byte[] wavBytes, string name) {
        string tmpWav = Path.GetTempFileName() + ".wav";
        string tmpOgg = Path.GetTempFileName() + ".ogg";
        try {
            File.WriteAllBytes(tmpWav, wavBytes);
            bool isMusic = name.StartsWith("mus_", StringComparison.OrdinalIgnoreCase) || 
                           name.StartsWith("song_", StringComparison.OrdinalIgnoreCase) ||
                           wavBytes.Length > 500000;

            // Ultra-compressed audio for PS Vita:
            // For music: 20kHz stereo 32kbps (extremely lightweight)
            // For SFX: 16kHz mono 24kbps (tiny size, zero CPU overhead)
            string args = isMusic 
                ? $"-y -nostats -loglevel quiet -i \"{tmpWav}\" -ar 20000 -ac 2 -c:a libvorbis -b:a 32k \"{tmpOgg}\""
                : $"-y -nostats -loglevel quiet -i \"{tmpWav}\" -ar 16000 -ac 1 -c:a libvorbis -b:a 24k \"{tmpOgg}\"";

            var psi = new ProcessStartInfo {
                FileName = "ffmpeg",
                Arguments = args,
                UseShellExecute = false,
                CreateNoWindow = true
            };
            using var proc = Process.Start(psi);
            proc.WaitForExit();
            if (File.Exists(tmpOgg)) {
                byte[] oggBytes = File.ReadAllBytes(tmpOgg);
                if (oggBytes.Length > 4 && oggBytes[0] == (byte)'O' && oggBytes[1] == (byte)'g') {
                    return oggBytes;
                }
            }
        } finally {
            if (File.Exists(tmpWav)) File.Delete(tmpWav);
            if (File.Exists(tmpOgg)) File.Delete(tmpOgg);
        }
        return wavBytes;
    }

    static void Main(string[] args) {
        bool forVita = args.Length > 0 && args[0].Equals("--vita", StringComparison.OrdinalIgnoreCase);
        string srcWinPath = @"Post Void Steam\data.win";
        string outPath = forVita 
            ? @"vita_port\postvoid\game.droid" 
            : @"android\app\src\main\assets\game.droid";

        Console.WriteLine($"[PostVoidCLI] Loading original data.win from {srcWinPath} (Target: {(forVita ? "PS Vita" : "Android (Pristine Full Quality)")})");
        UndertaleData data;
        using (var stream = File.OpenRead(srcWinPath)) {
            data = UndertaleIO.Read(stream);
        }
        Console.WriteLine($"Loaded data.win! Scripts: {data.Scripts.Count}, Code: {data.Code.Count}, Sounds: {data.Sounds.Count}");

        Console.WriteLine("Applying bulletproof Steam neutralization patches...");

        // 1. obj_platform_steam_Create_0:
        var steamCreateCode = data.Code.ByName("gml_Object_obj_platform_steam_Create_0");
        if (steamCreateCode != null) {
            steamCreateCode.Instructions[18].Kind = UndertaleInstruction.Opcode.PushI;
            steamCreateCode.Instructions[18].Type1 = UndertaleInstruction.DataType.Int16;
            steamCreateCode.Instructions[18].ValueShort = -1;
            steamCreateCode.Instructions[18].ValueFunction = null;

            steamCreateCode.Instructions[22].Kind = UndertaleInstruction.Opcode.PushI;
            steamCreateCode.Instructions[22].Type1 = UndertaleInstruction.DataType.Int16;
            steamCreateCode.Instructions[22].ValueShort = 0;
            steamCreateCode.Instructions[22].ValueFunction = null;
            Console.WriteLine("Patched obj_platform_steam_Create_0: steam_inventory & steam_initialised neutralized");
        }

        // 2. obj_platform_steam_Step_0:
        var steamStepCode = data.Code.ByName("gml_Object_obj_platform_steam_Step_0");
        if (steamStepCode != null) {
            steamStepCode.Instructions[0].Kind = UndertaleInstruction.Opcode.Exit;
            steamStepCode.Instructions[0].Type1 = UndertaleInstruction.DataType.Int32;
            steamStepCode.Instructions[0].ValueFunction = null;
            Console.WriteLine("Patched obj_platform_steam_Step_0: steam_update neutralized with Exit");
        }

        // 3. gml_GlobalScript_Achievements:
        var achCode = data.Code.ByName("gml_GlobalScript_Achievements");
        if (achCode != null) {
            achCode.Instructions[8].Kind = UndertaleInstruction.Opcode.B;
            achCode.Instructions[37].Kind = UndertaleInstruction.Opcode.B;
            achCode.Instructions[89].Kind = UndertaleInstruction.Opcode.B;
            Console.WriteLine("Patched gml_GlobalScript_Achievements: all Steam achievement/stat calls bypassed with B");
        }

        // 4. gml_GlobalScript_scr_new_highscore: Replace 115 with -1
        var hsCode = data.Code.ByName("gml_GlobalScript_scr_new_highscore");
        if (hsCode != null) {
            int count = 0;
            foreach (var inst in hsCode.Instructions) {
                if (inst.Kind == UndertaleInstruction.Opcode.PushI && inst.ValueShort == 115) {
                    inst.ValueShort = -1;
                    count++;
                }
            }
            Console.WriteLine($"Patched {count} instances of 115 in gml_GlobalScript_scr_new_highscore");
        }

        if (forVita) {
            Console.WriteLine("[PS Vita Mode] Applying 20% viewport downscaling and ultra-compressed audio...");
            foreach (var room in data.Rooms) {
                if (room.Views != null) {
                    foreach (var view in room.Views) {
                        if (view.PortWidth > 0 && view.PortHeight > 0) {
                            view.PortWidth = (int)(view.PortWidth * 0.80);
                            view.PortHeight = (int)(view.PortHeight * 0.80);
                        }
                        if (view.ViewWidth > 0 && view.ViewHeight > 0) {
                            view.ViewWidth = (int)(view.ViewWidth * 0.80);
                            view.ViewHeight = (int)(view.ViewHeight * 0.80);
                        }
                    }
                }
            }

            int convertedCount = 0;
            foreach (var sound in data.Sounds) {
                if (sound.AudioFile?.Data != null && sound.AudioFile.Data.Length > 4) {
                    byte[] oggBytes = ConvertWavToOgg(sound.AudioFile.Data, sound.Name?.Content ?? "");
                    if (oggBytes != sound.AudioFile.Data) {
                        sound.AudioFile.Data = oggBytes;
                        sound.Type = data.Strings.MakeString(".ogg");
                        sound.Flags = UndertaleSound.AudioEntryFlags.IsEmbedded | 
                                      UndertaleSound.AudioEntryFlags.IsCompressed | 
                                      UndertaleSound.AudioEntryFlags.Regular;
                        string fileName = sound.File?.Content ?? (sound.Name?.Content + ".ogg");
                        if (fileName.EndsWith(".wav", StringComparison.OrdinalIgnoreCase)) {
                            fileName = Path.ChangeExtension(fileName, ".ogg");
                        }
                        sound.File = data.Strings.MakeString(fileName);
                        convertedCount++;
                    }
                }
            }
            Console.WriteLine($"Converted {convertedCount} sounds to OGG Vorbis for PS Vita.");
        } else {
            Console.WriteLine("[Android Mode] Preserving 100% original full-fidelity audio & native full resolution (Zero compromises).");
        }

        // Save output
        Console.WriteLine("Saving game.droid to " + outPath);
        string dir = Path.GetDirectoryName(outPath);
        if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir)) {
            Directory.CreateDirectory(dir);
        }
        string tmpPath = outPath + ".tmp";
        using (var outStream = File.Create(tmpPath)) {
            UndertaleIO.Write(outStream, data);
        }
        File.Move(tmpPath, outPath, true);
        Console.WriteLine($"[PostVoidCLI] Saved successfully to {outPath}!");
    }
}

