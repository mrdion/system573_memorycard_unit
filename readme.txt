メモリーカードユニット互換機

arduino Megaで作る互換機です。

付属のソフトでWindows10でplaystationのメモリーカードを読み書きできます。

動作確認済み
Dance Dance Revolution 2ndMIX LINK Ver. (Memory card)
Dance Dance Revolution 3rdMIX (Memory card)
GUITARFREAKS 3rdMIX　(Memory card/playstation CONTROLLER)
GUITARFREAKS 4thMIX　(Memory card/playstation CONTROLLER)


[使いかた]
1.ksys573_white_io_jvs.png/ksys573_white_io_PS.pngを参考にして作成してください。
2.arduino_writer.rarを解凍し、write.batを以下のように環境に合わせて書き換えてください。

-PCOM5     -> Arduinoが接続されたポート
FW.ino.hex -> 書き込むFWのファイル名

3.write.batを実行して書き込みます


//内容物
[WhiteIO]
ksys573_white_IO_2.ino.mega.hexを書き込みsystem573で対応のゲームを起動させるとメモリーカードユニットとして動きます。
FWの書き込みとPCでメモリーカードのデータを読み書きする時のみArduino上のUSBポートを利用してください。


[SPI_CHECK]
ksys573_white_IO_SPI_CHECK.ino.mega.hexを書き込むと1P/2Pスロットに接続されたメモリーカードの0フレームを読み込みます。
正常に基板が接続されており正常に読み込めていると以下のデータが表示されます。


[MEMORY_CARD_READER]
ksys573_white_IO_2.ino.mega.hexを書き込んだArduino本体のUSBポートとPCを接続して使用します。
*劣化したり初期のメモリーカードなどは正常に読み込めない可能性があります。
*書き込みデータはヘッダなどがついていないデータを利用してください。

Open       -> シリアルポートを開きます。
Read       -> 1P側メモリーカードのデータをPCへ転送します。
Write      -> PCから1P側メモリーカードへデータを転送します。
ERROR_STOP -> チェックを入れてWriteを開始すると書き込みデータと比較を行い差異がある場合は停止させます。

mem_00.bin メモリーカードのデータを0x00で埋めたい場合に利用します。
mem_FF.bin メモリーカードのデータを0xFFで埋めたい場合に利用します。

PCへ転送したデータからゲームごとのブロックへ切り分けるなどは、以下のツールなどを利用してください。
http://shendosoft.blogspot.com/2014/01/memcardrex-18-released.html