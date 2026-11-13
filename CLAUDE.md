# Quantum Wave Hopper

C11 + libm の 1 次元時間依存シュレーディンガー方程式ソルバ(Crank–Nicolson)。物理の正典は `src/wave.c`。
端末版・CSV/SVG/GIF と、同じ C 計算を WASM で使うブラウザー観測画面(`src/observer.c` + `web/`)を持つ。

- 単位は ħ = m = 1。格子 3,900 点・Δx 0.1・Δt 0.05・障壁幅 2 は端末版と同じ。
- JavaScript は描画と入力だけ。波動関数の時間発展・確率・理論透過率は C で計算する。
- 原画の背景と案内役 Hop は装飾。Hop は密度の最大点に置くだけで、粒子の測定位置ではない(画面と README に明記)。
- 保存・サーバー API は無い。既存の出力(CSV/SVG/GIF)と物理モデルを保持する。乱数は物語の層(`src/farside.c`)と二重スリットの粒子(`sl_particle`)の計数器だけで、
  (seed, 夜, ダイヤル, 粒子番号)・(seed, 粒子番号)で引く番地式。ソルバは乱数を使わない。
- 検証は `make test`(解析解・保存則の照合 + adapter)/ `make bench`(native trace)/ `make plots` / `make web` / `make smoke`。
  native の cc が無い端末では、gcc の入ったコンテナで `make -j4 test bench quantum-wave-hopper` を実行して native を作り、
  Emscripten を有効にして `make web && node scripts/smoke.cjs` を手元で回す。
- 理論の手引きは `docs/theory/`。`make theory-figs` が図と実測値の表を `tests/theory_figs.c` から再生成し、`make theory-check` が
  対応表(文 → 頁 → 図 → 関数 → テスト)の欠け・リンク切れ・消えた関数名・未掲載の公開関数で落ちる。法則・規則を足したら同じ PR で頁と図と表を足す。
  README の「34 statements」は表の行数と合わせる。
- スマホ(幅 720px 以下・高さ 500px 以下・タッチ主体)では DOM と canvas の全文字を 14 CSS px 以上、操作を 44 CSS px 以上にする。`make smoke` が 390×844 と 844×390 で実測する。`window.__tmGate` は読み取り専用。
- CI は PR の軽量ゲート(docker build で `make test && make plots`)のみ。WASM ビルド・撮影・E2E はローカル専用。
- 公開 repo。コミット・PR に生成 AI の署名、私的な情報を入れない(公開前に private 情報の検査と差分の目視を行う)。
- 実画面は `docs/images/`(`make smoke` が撮る)、原画と生成指示・採否は `assets/art/` と `docs/art-direction.md`。

## 二重障壁(M2、Issue #2)

- 物理は `src/wave.c` の追加関数(転送行列 `layers_transmission`・共鳴探索 `double_barrier_resonance`・パケット平均・時間発展の実験)。既存関数と既存出力は変えない。
- ブラウザーの第二の実験は別アダプター `src/resonance.c`(`rs_*`、格子 11,000 点 x ∈ [−400, 700)・Δx 0.1)と `web/resonance.mjs`(描画だけ)。`make bench` が `build/resonance-trace.csv` を書き、`make smoke` が WASM と照合する。
- 端末出力は `./quantum-wave-hopper --resonance docs`(`docs/resonance.svg`・`docs/resonance.csv`、`make plots` に含む)。理論の手引きの 5 頁目(S20〜S25)。

## 二重スリット(M3、Issue #3)

- 2D の物理は別ファイル `src/wave2d.c`(交互方向の Crank–Nicolson: C_x(Δt/2) C_y(Δt) C_x(Δt/2)、各因子は Cayley 変換で厳密にユニタリ、Strang 対称で時間 2 次)。1D の `src/wave.c` と既存の出力は変えない。
- ブラウザーの第三の実験は `src/slits.c`(`sl_*`、256×256・Δx 0.2・Δt 0.02、壁 0.8 厚 V=60、吸収層 4)と `web/slits.mjs`(描画だけ。画像は C が RGBA で塗り `HEAPU8` で読む)。遠方スクリーンは x ≥ 1.8 の横運動量分布から計算する(L = 100 は仮想)。
- 粒子の着地は `sl_particle`(seed, 番号)の splitmix64 計数器で、ソルバには戻らない。点の横の散らばりは JS の固定ハッシュ(飾り)。
- `make bench` が `build/slits-trace.csv` を書き、`make smoke` が WASM と照合(値 1e-9・画像のバイト和)。端末出力は `./quantum-wave-hopper --slits docs`(`docs/slits.svg`・`.csv`・`.gif`、`make plots` に含む)。理論の手引きの 6 頁目(S26〜S34)。
- 2D の時間誤差は壁の中で大きい(V Δt = 1.2)が、遠方スクリーンは Δt を 1/4 にしても変わらない。厚い壁は Fraunhofer の薄い衝立ではない(S30)。

## 物語の層「向こう側」(M5、Issue #7)

- 場面・選択・数値は `src/farside.c`(C)、文は `web/story.mjs`(英日)。粒子が向こう側に見つかる確率は観測所の実行結果
  (`farside_transmission` = observer の P(x > barrier))そのもので、物語はそれを変えない(`tests/test_farside.c` の `[physics]` が Wave API とビット一致を確かめる)。
  物語が変えてよいのは一発の粒子数・電池・時間・アグネスの二度読み・電池切れの扱いだけ。
- 場面は進行の固定点(最初の夜の前と、夜を終える封)でだけ開く。重い場面(letter・fall・dark)は帯を出さない。
- `make story` が `docs/narrative/{play-order,ablation,tension}.json` を書き、`make test` が 8 行を再実行して一字一句の一致と種の重複なしを強制する。
  数値を変えたら `make story` で出し直す。beats.json の evidence は `web/story.mjs` の文そのまま。
- 理論の手引きの 4 頁目(S15〜S19)が物語の規則。

## M4 の独自原画の例外

ローカルの Stable Diffusion 1.5(stable-diffusion.cpp・CPU、API key なし)で生成した独自の原画
(観測所の背景・案内役ロボット Hop)を許可する。ブラウザーの背景・案内役の肖像・密度の上の目印に実描画する。
既存作品の名称・意匠、実在の人物、第三者の画像・外部音声・外部フォントは持ち込まない。原画を含め配布 10,000,000 B 以下を守る。
