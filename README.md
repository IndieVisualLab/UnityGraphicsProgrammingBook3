# IndieVisualLab執筆環境

## 利用ツール

### Re:View

Re:View使います

達人出版会の中の人が開発してるそう

#### インストール

https://github.com/kmuto/review

ここ見ればOK

### md2review

MarkDownを.reファイルに変換してくれるぞい

Gemfileに追加してるので
```bash
bundle install
```

で入るはず

### TeX

Re:View動かすのにTeXがいるのでインストール

#### Macの場合

http://www2.kumagaku.ac.jp/teacher/herogw/

ここからpTeXをDownloadしてくる（Sierra 10.12.5でいけた）

pTexを/Applications以下に移動して，
/Applications/pTeX.app/teTeX/binにPATHを通す．

.bashrcとかに
```bash
export PATH=$PATH:/Applications/pTeX.app/teTeX/bin
```
とか書けばOK

#### Macの場合 その２
上記の方法で出来なかった場合は  
http://qiita.com/hideaki_polisci/items/3afd204449c6cdd995c9

ここを参考にMacTexをインストールする。

.bashrcとかに
```bash
export PATH=$PATH:/Library/TeX/texbin
```
と書いてパスを通す

#### Windowsの場合 その1

Mr.Yata「Bash on Windows使えばいける」  

bash on windowsのインストールの仕方は下記参照  
http://qiita.com/Aruneko/items/c79810b0b015bebf30bb

bash on Windowsインストール後、bash上でRE:VIEWとTexをインストールする
```bash
sudo gem install review
sudo apt install texlive-full
sudo apt install build-essential
sudo apt-get install ruby-dev
sudo gem install md2review
```

#### Windowsの場合 その2

Bash on windowsではなく、RubyとMSYSを使う方法  
AtomやVSCodeのRe:Viewプラグイン等を使いたい場合はこの方法でやったほうがいい

http://kaiware007.hatenablog.jp/entry/2017/10/08/012738

## Re:Viewでの執筆フロー

### pdf化

```bash
review-pdfmaker config.yml
```
もしくは、

```bash
rake pdf
```

でPDF生成

### 章ごとのファイル追加

catalog.yml内に
```yaml
CHAPS:
    - Preface.re
    - Chapter1.re
    - Chapter2.re
```
という風に記述すると章ごとに使いたいファイルを指定できる．
    
### MarkDownからRe:Viewフォーマットに変換

```bash
md2review hoge.md > hoge.re
```
ってやるだけ




# サンプルコードリポジトリ
https://github.com/IndieVisualLab/UnityGraphicsProgramming