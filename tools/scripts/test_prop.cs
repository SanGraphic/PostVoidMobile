using System;
using System.Reflection;
using UndertaleModLib.Models;

class PropCheck {
    static void Main() {
        var t = typeof(UndertaleEmbeddedTexture.TexData);
        foreach (var p in t.GetProperties()) {
            Console.WriteLine("Prop: " + p.Name + " (" + p.PropertyType.Name + ")");
        }
    }
}
