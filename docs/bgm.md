# BGM(M63 #29、親 TM-59/project-context#598)

方針(project-context 20-stacks.md「ゲームの BGM」): 基調はバラード。戦闘の役と物語は別の曲で、場面の切り替えで替える。歌は文字を読まない場面だけ。

## 鳴らす経路

ブラウザー版(C のソルバを Emscripten で動かす観測画面)だけに Web Audio の再生を足した。物理は C のまま、JS は画面から曲を選ぶだけ。
端末版・CSV/SVG/GIF は無音のまま。曲は最初の操作の後に `bgm/list.json` と `bgm/<曲>.ogg` を読む(初期ロードに入れない)。読めない・復号できない曲は無音のまま遊びを止めない。

## 場面 → 曲

| 画面 | 曲 | 強さ |
|---|---|---|
| 1 次元の観測所(手動・Standard barrier・Exactly half)・二重障壁・二重スリット | lab(基調のバラード) | 1 |
| 物語「向こう側」の夜(ダイヤルを合わせて粒子を撃ち、封をするまで) | night(戦闘の役、pulse) | 1 |
| 夜の前後の場面(dusk・frost・radio) | story | 0.7 |
| 重い場面(letter・fall・dark) | story | 0.4 |
| dawn の場面(結末)と 5 夜を終えた後 | dawn | 1 |

戦う相手・主戦は無い。電池と時間で粒子を撃ち、向こう側を探す夜を戦闘の役にした。物語には一時停止が無い(次の一発を待つだけ)ので night は下げない。実験は押すまで止まっているのが普通なので一時停止で下げない。場面は選択を押すまで止まる(時間で流れない)。判定は `web/bgm.mjs` の `cue()`(物語か・場面・重さ・結末か・終わったかだけを読む)。

- 切り替えは 2.5 秒の交差フェード。
- **全部器楽**: どの場面も文字か計器を読むので、歌入りは作らない(英日の物語の文も器楽の下で読む)。
- 消音: 見出しの ♪(44px)と `M`(どちらも新設。M は他の操作に使われていない)。保存はしない(この repo は保存機能を持たない)。消音は出口(master)でも切るので、交差フェードで消えていく前の曲も同時に黙る(TM-59/project-context#668 の穴を最初から作らない)。

## 曲(採用・門の値)

| 場面(曲) | 鳴る画面 | 門 | 採用 | 通過 | ループ | LUFS / 山 | 継ぎ目の音色 | 落ち込み / 静かな窓 | 大きさ |
|---|---|---|---|---|---|---|---|---|---|
| lab | 基調(観測所・二重障壁・二重スリット) | soft | lab-c0-s952 | 10/12 | 18.75 秒 | -16.0 / -1.9 dBFS | 0.58 | 0.5 秒 / 0.054 | 413,565 B |
| night | 戦闘の役(向こう側 の 5 夜) | pulse 55.0 BPM(計画 110、半分の拍) | night-c1-s955(pick) | 7/18 | 35.014 秒 | -16.0 / -3.4 dBFS | 1.1 | 0.0 秒 / 0.0 | 842,282 B |
| story | 物語(夜の前後の場面。重い場面 letter・fall・dark は 0.4 倍) | soft | story-c1-s955 | 7/12 | 17.143 秒 | -16.0 / -3.0 dBFS | 0.69 | 0.0 秒 / 0.0 | 375,613 B |
| dawn | 結末(dawn の場面と 5 夜を終えた後) | soft | dawn-c0-s954 | 10/12 | 23.226 秒 | -15.9 / -1.6 dBFS | 0.69 | 1.0 秒 / 0.043 | 505,818 B |

計 2,137,278 B(4 曲)。通過 34/54。全候補の値は `docs/bgm/gate.md`、生の値は `docs/bgm/report.json`。

**night の pick**: 点数(継ぎ目の音色と拍)で選ばれた night-c1-s952 は 1 周 35 秒の頭の約 3.5 秒に拍が無かった(落ち込み 1.5 秒・静かな窓 0.058 で門は通る)。戦闘の役は鳴った瞬間に分かる必要があるので、門は変えずに、門を通った 7 本の中から頭から終わりまで拍が続く night-c1-s955(拍の強さ 0.773・落ち込み 0・継ぎ目の音色 1.1)を `docs/bgm/plan.json` の `pick` に書いて採った。`scripts/bgm_bake.py` は pick が門を落ちていれば点数で選ぶ(門を迂回しない)。

## 作り方

1. 計画: `docs/bgm/plan.json`(場面 × caption × seed 6、seed 951〜956。pulse の night だけ caption 3 つで、どれも頭から拍が続く指示。他の作品と同じ caption・seed を使わない)。caption は音の特徴だけで、曲名・作者・作品名を書かない。
2. 生成: ACE-Step 1.5 turbo(MIT、steps 8)を Colab の L4 で 1 回の確保(54 本、端末からの 1 回 351.8 秒、計算枠 587.14 → 587.0)。
   `cd ~/dev/local-llm-ops && .venv/bin/python -m tools.colab bgm --repo <この repo> --out <dir> --backend colab --gpu L4`
   Colab が使えない時の落ちる先は `scripts/bgm_gen.py`(手元の CPU、local-llm-ops の music venv)。
3. 門と書き出し: `~/.local/share/local-llm-ops/genvenv/music/bin/python scripts/bgm_bake.py --takes <dir>`(ffmpeg が要る)。
   門は gravity-well #35・spot-gap #33・td-games #38 と同じ値で緩めていない: soft(バラード・物語・結末)は local-llm-ops `tools/genmodels/mood.py` の soft の門、pulse(戦闘の役)は PULSE_GATE + テンポ ±5%、全部に -16 LUFS ±1.5・山 -1 dBFS・継ぎ目の音色の跳び 2 倍以下・12 dB の落ち込み 2 秒以下、pulse は静かな窓の割合 0.10 以下、soft のループは 16 秒以上。
   `scripts/bgm_bake.py`・`scripts/bgm_gen.py` は gravity-well #35 の物と同じ(bake に plan.json の pick を読む 3 行を足した。Ruff ALL・mypy strict を通る。soundfile・matplotlib を使わず ffmpeg で読み書き・作図)。mypy が numpy の型を読むので pyproject.toml の dev に numpy を足した。
4. 出所と利用条件: 曲は ACE-Step 1.5(MIT、出力に第三者の権利の主張なし)で手元の計画から生成した独自の音源。第三者の音源・歌詞・旋律は使っていない。外部の生成 AI・API key は使っていない。

## 波形とスペクトル(自分で見た所見)

画像は 2 周つないだ物で、真ん中が継ぎ目(上が波形、下が対数の周波数のスペクトル)。

- `lab`: 鍵盤と撥弦の句が交互に鳴り、高い帯の和音が句ごとに広がる。句の間の静けさは最長 0.5 秒。継ぎ目(0.58)の段差は見えない。
- `night`: c1-s955。頭から終わりまで等間隔の拍と低い帯が続き、薄い区間が無い(落ち込み 0・静かな窓 0)。点数で選ばれた c1-s952 は頭の約 3.5 秒が薄かった(上の pick)。
- `story`: 弦の持続と鍵盤の句がまとまって鳴り、句の切れ目で短く静まる(落ち込み 0)。台詞の下で邪魔をしない一定の薄さ。山 -3.0 dBFS。
- `dawn`: 前半は薄い鍵盤、後半に旋律の帯が厚くなる。継ぎ目の段差は見えない(0.69)。

人の試聴は門にしない(客観の門を通した物だけを採る)。

## 試験

- `node scripts/bgm-smoke.cjs`(ローカル専用、port `BGM_PORT` 既定 22093、記録 `docs/bgm/smoke.json`):
  最初の操作の前は何も読まない(0 件)・lab(出口 -25.3 dBFS)・dusk の場面で story(-28.2、0.7 倍)・交差フェード中の M で 0.8 秒後 -120 dBFS・night(-16.5、1 倍)・♪ 44×44 で -120 → -25.7・重い場面と dawn の cue・dist 2,443,227 B(10 MB の門の内。BGM 4 曲 2,137,278 B を含む。main は 296,613 B)。
- 既存の `make test`(解析解・保存則・adapter・物語の `story_bench --check` 8 行一致)と `make smoke`(149 項目、390×844・844×390 の文字と 44px を含む)は通る。撮り直した画像は戻した。
  ♪ を足した最初の版は 390×844 で見出しが 434px に広がり ♪ が画面の外に出たので、幅 720px 以下で飾りの矢印を隠して間を詰めた(390px に収まる)。
- 遊べる門(`bin/playability-gate.py`、main と branch を同じ時間帯 2026-11-27 10:06): main 2/7・branch 3/7。増えた 1 つは「タッチ受付」(曲の読み込みを始める pointerdown の受け手が増えただけで、遊びの操作は変わらない)。②タップで進行は main・branch とも未到達(物語はキーの Space で撃つ作り)、④⑤⑥ も main と同じ既知の型。
