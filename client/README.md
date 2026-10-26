# Yt2Client

Development client for the seam build of `SWREG.EXE`: it decodes the
protocol-v0 VIDEO and AUDIO streams the engine appends to the folder link
(`STREAM.BIN`, input back through `INPUT.BIN`), renders the picture with SDL3
(via P/Invoke - no NuGet packages) and plays the FM music through its own OPL2
synth.

    sh engine/asm/swreg/tools/build.sh --variant file   # build the engine side (folder seam)
    dotnet run --project client -c Release -- \
        --follow build/swreg/file/STREAM.BIN --input build/swreg/file/INPUT.BIN

Build the client + asm server bundle, then start it:

    sh tools/publish-linux.sh --server asm
    build/dist/yt2-linux-x64/play.sh

Headless live check (the folder-link files are timed directly):

    dotnet build/client/bin/Release/net11.0/Yt2Client.dll \
        --follow STREAM.BIN --headless --stats 5 --ppm out.ppm

Replay the timed output file at its captured pace:

    dotnet build/client/bin/Release/net11.0/Yt2Client.dll \
        --file build/dist/yt2-linux-x64/sample/STREAM.BIN --headless --ppm out.ppm

`--file` detects timed `YT2B` `.BIN` streams and optional YT2S sessions; raw v0
fixtures are decoded without pacing. The folder link writes timestamps into
`STREAM.BIN` and `INPUT.BIN`, so a separate session file is not required.
`--record-session PATH` explicitly adds a full-duplex YT2S capture.

`--help` lists every option (`--dump`, `--scale`, `--stats`, `--raw`,
`--no-keyframe`, `--no-audio`, `--dro`/`--wav` for an offline OPL capture, ...;
`--tcp` is the obsolete serial link). With a live link the window captures the mouse;
`Ctrl+M` releases it and `Ctrl+Q` quits, so does closing the window (which
also cancels a connect still waiting for the server).  If the server goes
away, the window stays and the client reconnects when it comes back.

Windows bundle (self-contained exe + SDL3 + the game side, ready to copy):

    sh tools/publish-win.sh --server asm  # -> build/dist/yt2-win-x64.zip

Details — the link, the nullmodem `0xFF` escape, verification numbers, debugging recipe — are in
[`docs/client.md`](../docs/client.md).
