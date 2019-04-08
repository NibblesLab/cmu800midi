# USB-MIDI I/F for Roland/Amdek CMU-800 by Arduino (cmu800midi)
今やビンテージ音源とも評されるAmdek/Roland DGのCMU-800をUSB-MIDI化するプロジェクトです。
## 特徴
- RJBさん作2011モデルと同等の機能を持ちます。
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

もあさん作USB-MIDI版ではCMU-800の電源を直接監視して投入状態を検知していましたが、ケーブルには電源は来ていないので、データバスがプルアップされているのを利用してD0信号を監視するように変更しています。回路図中の82kΩ抵抗は、端子が電気的に開放状態になっている時に確実に'L'レベルにするためにあります。プルアップ抵抗が10kΩと大きめなので、影響を最小限にできるようこちらも大きめの抵抗値を選択しました。
## ソフト
基本的にはもあさん作USB-MIDI版の移植です。MsTimer2とMIDIのライブラリを使用しています。CMU-800の制御やチャンネル9のポリフォニック処理はそのままですが、ArduinoやMIDIライブラリの流儀に従った記述に書き換えました。

## 参考
