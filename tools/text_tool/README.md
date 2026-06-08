# Text Tool

KST32Bストロークフォントデータから、日本語文字列をペンプロッタ用G-codeへ変換するCLIツールです。
Inkscape GUIやHershey Textには依存せず、コマンドラインだけで変換します。

## Font

KST32BはJIS漢字などを30x32程度の格子点ストロークで定義したCSF/1形式のフォントデータです。
フォント本体は配布元から取得し、`KST32B.TXT`を以下へ配置して使います。

```text
tools/text_tool/fonts/KST32B.TXT
```

配布元:

- KST32B Vector配布ページ: https://www.vector.co.jp/soft/dl/data/writing/se119277.html
- KST32B v3 zip URL: https://ftp.vector.co.jp/69/13/114/KST32Bv3.zip

`tools/text_tool/fonts/`内のフォント本体は`.gitignore`で除外しています。ローカル実行用に配置しますが、リポジトリへはコミットしません。

## Generate

```bash
python tools/text_tool/kst32b_to_gcode.py \
  --font tools/text_tool/fonts/KST32B.TXT \
  --text "ロボット" \
  --x 10 \
  --y 10 \
  --size 20 \
  --char-spacing 3 \
  --feed 3000 \
  --rapid-feed 8000 \
  --dwell-ms 80 \
  --max-x 55 \
  --max-y 55 \
  --auto-scale-to-fit \
  -o tools/text_tool/examples/gcode/text_robo.gcode
```

テキストファイルから変換する場合:

```bash
python tools/text_tool/kst32b_to_gcode.py \
  --font tools/text_tool/fonts/KST32B.TXT \
  --input-file tools/text_tool/examples/text_konnichiwa.txt \
  --max-x 55 \
  --max-y 55 \
  --auto-scale-to-fit \
  -o tools/text_tool/examples/gcode/text_konnichiwa.gcode
```

## Output

出力G-codeは既存ファームウェアの最小G-codeサブセットに合わせています。

```text
G21
G90
G0
G1
M3
M5
G4 P<ms>
```

`G0`はペンアップ移動、`G1`は描画移動、`M3`はペンダウン、`M5`はペンアップとして出力します。
座標はmm、absolute positioningです。CoreXY変換、soft limit、planner、TMC2209制御はファームウェア側で処理します。

## Send

生成したG-codeはSerial Toolの`--gcode`で送信できます。実機を動かす前に、`--preamble-csv`でalarm clear、limit確認、homingを前置してください。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --preamble-csv tools/serial_tool/examples/gcode_preamble.csv \
  --gcode tools/text_tool/examples/gcode/text_robo.gcode \
  --startup-delay 4 \
  --queue-mode \
  --stream-gcode-motion \
  --echo
```

## Notes

- KST32Bの座標は`--size`で指定した1文字高さmmへスケーリングします。
- `--max-x`、`--max-y`、`--auto-scale-to-fit`を指定すると、生成G-codeが指定範囲内へ収まるように文字サイズを自動縮小します。
- 文字間は`--char-spacing`、改行後の行間は`--line-spacing`で調整します。
- 文字が上下反転する場合は`--flip-y`を付けてください。
- 濁点、半濁点、小さい「ゃゅょっ」などの短いストロークは、実機ではペン先径や紙質で潰れやすいです。
- ペン上下後の`--dwell-ms`を調整すると、インクの出始めや線の抜けが改善する場合があります。
- 細かいG-codeが線分ごとに止まって見える場合は、Serial Toolの`--stream-gcode-motion`を使って`G0/G1`をqueue投入ACKで先行送信してください。stream対象行では送信速度を優先して成功時の行別ログが抑制されます。ストローク間の停止が気になる場合は`--dwell-ms`も下げて調整します。
- 未対応文字は警告をstderrへ出し、既定では代替の四角形グリフを出力します。スキップしたい場合は`--missing-glyph skip`を使います。
- 余計な線分簡略化や形状変更は行いません。
