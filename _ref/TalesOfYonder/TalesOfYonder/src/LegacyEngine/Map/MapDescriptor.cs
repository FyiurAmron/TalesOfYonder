namespace TalesOfYonder.LegacyEngine.Map {

public class MapDescriptor {
    public string suffix; // [3]
    public int nameIndex;
    public int type; // 01 or 02

    public string fullName; // computed

    public override string ToString() => fullName;
}

}
