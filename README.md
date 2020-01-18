# USB-MIDI I/F for Roland/Amdek CMU-800 by Arduino (cmu800midi)
今やビンテージ音源とも評されるAmdek/Roland DGのCMU-800をUSB-MIDI化するプロジェクトです。RJBさん([@RadioJunkBox](https://twitter.com/radiojunkbox))オリジナルのMIDI I/Fをもあさん([@morecat_lab](https://twitter.com/morecat_lab))がUSB-MIDI化したものを、Arduinoに移植しました。
## 特徴
- RJBさん作2011モデルとDIN-SYNCを除き同等の機能を持ちます。
- 本体を改造する必要はありません。
- CMU-800とI/Fとの電源投入順序を問いません。
## 必要な物
- CMU-800
- Arduino Uno及び互換機(USBコントローラにAVRを使用したもの)
- バニラシールドなどのユニバーサル基板
- 50ピンカードエッジコネクタ基板
## シールド
RJBさん作MIDI I/Fと同様に、CMU-800から伸びるケーブルの先のカードエッジコネクタに取り付ける形態を採用しました。カードエッジコネクタ部はArduinoのシールドとして製作しています。

![shield](https://github.com/NibblesLab/cmu800midi/blob/master/cmu800midi.PNG)

[電子部品通販 M.A.D.製MSX用ユニバーサル基板](https://la04528673.shop-pro.jp/?pid=100748823)を使用しましたが、Arduinoの端子配置が特殊なので[サンハヤト製Arduino用ユニバーサル基板 UB-ARD03](https://www.sunhayato.co.jp/material2/index.php/item?cell003=%E3%83%A6%E3%83%8B%E3%83%90%E3%83%BC%E3%82%B5%E3%83%AB%E5%9F%BA%E6%9D%BF%E8%A3%BD%E5%93%81&cell004=Arduino%E7%94%A8%E5%9F%BA%E6%9D%BF&name=Arduino%E7%94%A8%E3%83%A6%E3%83%8B%E3%83%90%E3%83%BC%E3%82%B5%E3%83%AB%E5%9F%BA%E6%9D%BF+UB-ARD03&id=505&label=1)を間に噛ませてあります。

![shield](http://retropc.net/ohishi/museum/cmu800midi_1.jpg)
![shield](http://retropc.net/ohishi/museum/cmu800midi_2.jpg)

もあさん作USB-MIDI版ではCMU-800の電源を直接監視して投入状態を検知していましたが、ケーブルには電源は来ていないので、データバスがプルアップされているのを利用してD0信号を監視するように変更しています。回路図中の82kΩ抵抗は、端子が電気的に開放状態になっている時に確実に'L'レベルにするためにあります。プルアップ抵抗が10kΩと大きめなので、影響を最小限にできるようこちらも大きめの抵抗値を選択しました。
## ソフト
基本的にはもあさん作USB-MIDI版の移植です。MsTimer2とMIDIのライブラリを使用しています。CMU-800の制御やチャンネル9のポリフォニック処理はそのままですが、ArduinoやMIDIライブラリの流儀に従った記述に書き換えました。

CMU-800の電源監視は、データバスのD0の向きを一時的に入力にして、その値を読むことで行います。電源が入っていると内部のプルアップ抵抗の効果により'H'が見える(1と読める)はずです。電源が入っていない時はシールドのプルダウン抵抗の効果の方が大きくなり'L'が見える(0と読める)はずです。

またオリジナルに対する追加機能として、システムメッセージのチューンリクエスト(0xF6)にチューニング用トーンの発生機能を割り当てました。トグル動作となっており、一度受信するとトーンが発生し、もう一度受信すると止まります。トーンを発生させてCMU-800本体のTUNEのキャップの中にあるトリマーをドライバーで回し、440Hzに合わせて下さい。

さらに、シールドに追加したミニジャックとCMU-800のクロック出力を接続することで、システムメッセージのMIDIクロックを発生する機能も追加しました。CMU-800のTEMPOツマミを操作すると、MIDIクロックの間隔が変化します。クロック出力が不要な時はクロックのケーブルをどちらか片方でも外すと止まります。

なお、ArduinoのUSB-MIDI化にはもあさんのMocoLUFAを使用しています。当然ながら、MocoLUFAを使用しなくてもMIDIシールド等と併用してUSBではないMIDIによる接続も可能です。
## 参考
- [#052 CMU-800 MIDI interface](http://beatnic.jp/products/cmu-800-midi-interface/) ([beatnic.jp](http://beatnic.jp/))
- [CMU-800 USB-MIDIインタフェースを作る](http://morecatlab.akiba.coocan.jp/lab/index.php/2012/04/cmu-800-usb-midi/) ([morecat_lab](http://morecatlab.akiba.coocan.jp/lab/))
- [Midi Firmware for Arduino Uno (Moco)](http://morecatlab.akiba.coocan.jp/lab/index.php/aruino/midi-firmware-for-arduino-uno-moco/) (morecat_lab)
## ご注意
- CMU-800が発声可能なチャンネル数を超えるデータや、チャンネル9以外にポリフォニック発音を期待した演奏を送り込んでもエラーにはなりませんが、まともな曲にはなりません。チャンネル数を絞り込みましょう。
- データが多すぎると演奏がもたつく可能性があります。CPUクロックを20MHzに交換すると改善されるかもしれません。
- CMU-800は製造から30年以上を経過していることもあり、何らかの故障箇所があるかもしれません。MIDI I/Fがちゃんと動いても音が出ない可能性があります。私のCMU-800も故障箇所の修理のためIC(4011,8253)やスライドボリュームを交換し、延命化のため三端子レギュレータや電解コンデンサを交換し、端子の洗浄なども施しました。ビンテージ機器ですから、相応のメンテナンスが必要になることも覚悟しておいて下さい。
## 謝辞
先行する素晴らしいアイテムを製作されたRJBさん・もあさんに感謝いたします。
