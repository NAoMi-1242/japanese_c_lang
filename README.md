# <img src="docs/images/jpc-icon.png" height="35" style="vertical-align: bottom;"> jpc言語
## 資料
[仕様書はこちら](docs/specification_document.md)

[スライドはこちら（Canvaに飛びます）](https://www.canva.com/design/DAG5nal8uAE/RtLvWdrWG5e74CiZr3suuw/edit?utm_content=DAG5nal8uAE&utm_campaign=designshare&utm_medium=link2&utm_source=sharebutton)

## 実行方法

### 1. リポジトリのクローン
```bash
git clone https://github.com/NAoMi-1242/japanese_c_lang.git
````

### 2. ディレクトリの移動
```bash
cd japanese_c_lang
```

### 3. Dockerイメージのビルド
```bash
docker-compose build
```

### 4. コンテナの起動
```bash
docker-compose run --rm dev
```

### 5. jpcのコンパイル
```bash
make
```

### 6. 実行
```bash
./jpc tests/sample.jpc -o sample
./sample
```

## 実行デモ動画
<video src="docs/demo.mp4" controls></video>