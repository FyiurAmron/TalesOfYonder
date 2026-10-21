# Yt2Client

Development client for the seam build of `SWREG.EXE`: it decodes the protocol-v0 VIDEO stream the
engine sends over COM1 and renders it with SDL3 (via P/Invoke — no NuGet packages).

    sh engine/asm/swreg/tools/build.sh --variant serial      # build the engine side (serial seam)
    client/server.sh                  # DOSBox-X window, game starts
    dotnet run --project client -c Release      # in second terminal

Headless check / replay:

    dotnet build/client/bin/Release/net11.0/Yt2Client.dll --tcp 127.0.0.1:2324 --headless --ppm out.ppm
    dotnet build/client/bin/Release/net11.0/Yt2Client.dll --file STREAM.BIN --headless --ppm out.ppm

`--help` lists every option (`--dump`, `--scale`, `--stats`, `--raw`,
`--no-keyframe`, ...). With a live link the window captures the mouse;
`Ctrl+M` releases it and `Ctrl+Q` quits, so does closing the window (which
also cancels a connect still waiting for the server).  If the server goes
away, the window stays and the client reconnects when it comes back.

Windows bundle (self-contained exe + SDL3 + the game side, ready to copy):

    sh client/publish-win.sh          # -> build/dist/yt2-win-x64.zip

Details — the link, the nullmodem `0xFF` escape, verification numbers, debugging recipe — are in
[`docs/client.md`](../docs/client.md).
