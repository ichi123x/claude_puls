# tasklist.md — タスク一覧

> **運用ルール**
> - タスク完了時は `[ ]` を `[x]` に変更する
> - 新しいタスクは該当フェーズの末尾に追加する
> - 優先度：🔴 高 / 🟡 中 / 🟢 低

---

## フェーズ1：仕様定義

- [x] 🔴 `.steering/product.md` にゴール・要件を記入する
- [x] 🔴 `.steering/structure.md` に技術スタックを記入する
- [x] 🟡 スコープ（In / Out）を確定する
- [x] 🟡 完了条件を具体的に定義する

## フェーズ2：設計

- [x] 🔴 ディレクトリ構成を確定し `structure.md` に反映する
- [x] 🔴 データフローを図示する
- [x] 🟡 命名規則・コーディング規約を決める
- [x] 🟢 `decisions.md` に初期設計判断を記録する（ADR-001〜005）

## フェーズ3：実装（仕様は同一のまま実装をゼロから書き直し済み）

- [x] 🔴 ソリューション・プロジェクトファイルを作成する（`ClaudePulse.sln` / `ClaudePulse.vcxproj`、x64 / Unicode / MFC共有DLL / C++17 / `/utf-8`）
- [x] 🔴 リソースを作成する（`Resource.h` / `ClaudePulse.rc`：メインダイアログ・バージョン情報。`Resource.h` はASCIIのみ）
- [x] 🔴 アプリ骨格を実装する（`pch` / `framework.h` / `targetver.h` / `ClaudePulse.h/.cpp`）
- [x] 🔴 使用量監視を実装する（`UsageMonitor.h/.cpp`：最新JSONL特定・追尾読み・model/usage抽出・集計）
- [x] 🔴 パルスメータを実装する（`PulseMeterCtrl.h/.cpp`：ダブルバッファ・心電図風波形・対数正規化）
- [x] 🔴 メインダイアログを実装する（`ClaudePulseDlg.h/.cpp`：1秒タイマー・ラベル更新・ダークテーマ）
- [x] 🟡 `.gitignore` に VS ビルド成果物を追加する
- [x] 🔴 プラン使用制限の表示を Claude Desktop と同形式にする（ADR-006）
  - [x] 共通JSONヘルパーを新設し `UsageMonitor` をリファクタリング（`JsonLite.h`）
  - [x] 使用量エンドポイントの取得を実装（`UsageLimitsClient.h/.cpp`：credentials.json 読取・WinHTTP・ISO8601解析）
  - [x] 使用率プログレスバーを実装（`UsageBarCtrl.h/.cpp`：角丸バー・70%/90% で色変化）
  - [x] ダイアログに使用制限セクションを追加（3行のバー表示・60秒間隔・ワーカースレッド取得）
- [x] 🟡 最前面表示／通常表示のラジオボタンを追加する（ADR-007）
  - [x] `Resource.h` / `ClaudePulse.rc` にラジオボタン2つとラベルを追加する
  - [x] `ClaudePulseDlg` にクリックハンドラーを実装し `SetWindowPos` でZオーダーを切り替える
  - [x] 標準ボタンがダークテーマで見えない問題を修正（`RadioDotCtrl` による自前描画に変更）
  - [x] 選択状態をレジストリへ永続化し、次回起動時に復元する（既定は「最前面に表示」）

## フェーズ4：テスト・確認

- [ ] 🔴 VS2022（x64）でビルドし、エラーがあればテキストで共有する
- [ ] 動作確認（手動）：Claude Code 実行中にモデル名・使用量・波形が更新されること
- [ ] 動作確認（手動）：プラン使用制限が Claude Desktop の設定画面と同じ%で表示されること
- [ ] エラーハンドリングの確認：`.claude\projects` が無い環境で待機表示になること
- [ ] エラーハンドリングの確認：未ログイン／オフライン時に使用制限がエラー表示のみで済むこと
- [ ] ドキュメント最終更新

## フェーズ5：完了

- [ ] `product.md` の成功条件をすべて満たしていることを確認
- [x] READMEを作成・更新する
- [ ] GitHubにプッシュ

---

## 完了済み

<!-- 完了タスクをここに移動（任意） -->
