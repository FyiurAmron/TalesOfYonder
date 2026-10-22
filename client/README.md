# Yt2Client

Development client for the seam build of `SWREG.EXE`: it decodes the protocol-v0 VIDEO stream the
engine appends to the folder link (`STREAM.BIN`, input back through `INPUT.BIN`) and renders it
with SDL3 (via P/Invoke — no NuGet packages).

    sh engine/asm/swreg/tools/build.sh --variant file   # build the engine side (folder seam)
    dotnet run --project client -c Release -- \
        --follow build/swreg/file/STREAM.BIN --input build/swreg/file/INPUT.BIN

Or one command from a bundle: `sh client/publish-linux.sh`, then
`build/dist/yt2-linux-x64/play.sh`.

Headless check / replay:

    dotnet build/client/bin/Release/net11.0/Yt2Client.dll \
        --follow STREAM.BIN --headless --stats 5 --ppm out.ppm
    dotnet build/client/bin/Release/net11.0/Yt2Client.dll --file STREAM.BIN --headless --ppm out.ppm

`--help` lists every option (`--dump`, `--scale`, `--stats`, `--raw`, `--no-keyframe`, ...;
`--tcp` is the obsolete serial link). With a live link the window captures the mouse;
`Ctrl+M` releases it and `Ctrl+Q` quits, so does closing the window (which
also cancels a connect still waiting for the server).  If the server goes
away, the window stays and the client reconnects when it comes back.

Windows bundle (self-contained exe + SDL3 + the game side, ready to copy):

    sh client/publish-win.sh          # -> build/dist/yt2-win-x64.zip

Details — the link, the nullmodem `0xFF` escape, verification numbers, debugging recipe — are in
[`docs/client.md`](../docs/client.md).
