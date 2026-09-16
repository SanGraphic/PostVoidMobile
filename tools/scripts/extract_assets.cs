using System;
using System.IO;
using UndertaleModLib;
using UndertaleModLib.Models;

class AssetExtractor {
    static void Main(string[] args) {
        string dataPath = @"Post Void Steam\data.win";
        Console.WriteLine("[AssetExtractor] Reading " + dataPath + "...");
        using var stream = File.OpenRead(dataPath);
        var data = UndertaleData.Create(stream);

        // Export All Texture Pages (Embedded PNGs)
        string texDir = @"decompiled_project\textures";
        Directory.CreateDirectory(texDir);
        for (int i = 0; i < data.EmbeddedTextures.Count; i++) {
            var tex = data.EmbeddedTextures[i];
            if (tex.TextureData?.TextureBlob != null) {
                string texPath = Path.Combine(texDir, $"texture_{i}.png");
                File.WriteAllBytes(texPath, tex.TextureData.TextureBlob);
            }
        }
        Console.WriteLine($"[AssetExtractor] Exported {data.EmbeddedTextures.Count} embedded texture pages.");

        // Export All Sounds (WAV/OGG)
        string soundDir = @"decompiled_project\sounds";
        Directory.CreateDirectory(soundDir);
        int soundExported = 0;
        foreach (var snd in data.Sounds) {
            if (snd.AudioFile?.Data != null && snd.AudioFile.Data.Length > 0) {
                string ext = ".ogg";
                byte[] d = snd.AudioFile.Data;
                if (d.Length > 4 && d[0] == 'R' && d[1] == 'I' && d[2] == 'F' && d[3] == 'F') {
                    ext = ".wav";
                }
                string sndPath = Path.Combine(soundDir, $"{snd.Name.Content}{ext}");
                File.WriteAllBytes(sndPath, snd.AudioFile.Data);
                soundExported++;
            }
        }
        Console.WriteLine($"[AssetExtractor] Exported {soundExported} audio files.");
    }
}
