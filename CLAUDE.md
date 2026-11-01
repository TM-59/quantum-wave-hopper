# Quantum Wave Hopper

C11 + libm の 1 次元時間依存シュレーディンガー方程式ソルバ(Crank–Nicolson)。物理の正典は `src/wave.c`。
端末版・CSV/SVG/GIF と、同じ C 計算を WASM で使うブラウザー観測画面(`src/observer.c` + `web/`)を持つ。

- 単位は ħ = m = 1。格子 3,900 点・Δx 0.1・Δt 0.05・障壁幅 2 は端末版と同じ。
- JavaScript は描画と入力だけ。波動関数の時間発展・確率・理論透過率は C で計算する。
- 原画の背景と案内役 Hop は装飾。Hop は密度の最大点に置くだけで、粒子の測定位置ではない(画面と README に明記)。
- 乱数・保存・サーバー API は無い。既存の出力(CSV/SVG/GIF)と物理モデルを保持する。
- 検証は `make test`(解析解・保存則の照合 + adapter)/ `make bench`(native trace)/ `make plots` / `make web` / `make smoke`。
  native の cc が無い端末では、gcc の入ったコンテナで `make -j4 test bench quantum-wave-hopper` を実行して native を作り、
  Emscripten を有効にして `make web && node scripts/smoke.cjs` を手元で回す。
- CI は PR の軽量ゲート(docker build で `make test && make plots`)のみ。WASM ビルド・撮影・E2E はローカル専用。
- 公開 repo。コミット・PR に生成 AI の署名、私的な情報を入れない(公開前に private 情報の検査と差分の目視を行う)。
- 実画面は `docs/images/`(`make smoke` が撮る)、原画と生成指示・採否は `assets/art/` と `docs/art-direction.md`。

## M4 の独自原画の例外

ローカルの Stable Diffusion 1.5(stable-diffusion.cpp・CPU、API key なし)で生成した独自の原画
(観測所の背景・案内役ロボット Hop)を許可する。ブラウザーの背景・案内役の肖像・密度の上の目印に実描画する。
既存作品の名称・意匠、実在の人物、第三者の画像・外部音声・外部フォントは持ち込まない。原画を含め配布 10,000,000 B 以下を守る。
