namespace TalesOfYonder.LegacyEngine.Gfx {

using System.Collections.Generic;

public record PictureGroup(
    PictureGroupDescriptor pictureGroupDescriptor,
    List<Picture> pictures
) {
}

}
