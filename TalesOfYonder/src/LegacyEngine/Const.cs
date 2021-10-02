namespace TalesOfYonder.LegacyEngine {

public class Const {
    public const string YT2_PICTURE_FILENAME = "PICTURES.VGA";
    public const string DEFAULT_PALETTE_NAME = "game.pal";

    public static readonly string[] paletteFilenames = {
        DEFAULT_PALETTE_NAME,
        "logo.pal",
        "webfoot.pal"
    };

    public static readonly PictureGroupDescriptor[] yt2PictureGroupDescriptors = {
        new() {
            description = "full-screen UI/intro pics",
            picWidth = 318,
            picHeight = 198,
            picCount = 23,
            paletteSelector = new() {
                [0] = "logo.pal",
                [16] = "webfoot.pal"
            }
        },
        new() {
            description = "large 'pane style' UI pics",
            picWidth = 210,
            picHeight = 105,
            picCount = 101
            // 6 pics for anim of a guy entering a room?
        },
        new() {
            description = "'tall' enemies",
            picWidth = 140,
            picHeight = 155,
            picCount = 215
        },
        new() {
            description = "'wide' enemies",
            picWidth = 190,
            picHeight = 110,
            picCount = 162
        },
        new() {
            description = "ground textures",
            picWidth = 224,
            picHeight = 74,
            picCount = 18
        },
        new() {
            description = "ceiling textures",
            picWidth = 224,
            picHeight = 62,
            picCount = 12
        },
        new() {
            description = "PC paperdolls & misc vertical gfx",
            picWidth = 56,
            picHeight = 136,
            picCount = 55
        },
        new() {
            description = "spell & shop icons, faces, paperdoll equips",
            picWidth = 32,
            picHeight = 32,
            picCount = 270
        },
        new() {
            description = "UI & inventory icons",
            picWidth = 16,
            picHeight = 16,
            picCount = 510
        },
        new() {
            description = "map & status icons + rings (inv/paperdoll)",
            picWidth = 8,
            picHeight = 8,
            picCount = 576
        }
    };
}

}
