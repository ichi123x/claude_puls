# structure.md — アーキテクチャ・技術スタック

> **用途**：技術的な構成・設計方針を定義する。
> Claudeは実装前にこのファイルを確認し、方針に従ったコードを書くこと。

---

## 技術スタック

| 項目 | 採用技術 | 理由 |
|------|----------|------|
| 言語 | C++17 | 組み込み系との親和性 |
| フレームワーク | MFC (VS2022 / v143) | ダイアログベースUI |
| 文字コード | Unicode (_UNICODE) + ソースは UTF-8（`/utf-8` オプション） | 日本語対応 |
| ビルド | x64 Debug / Release | 開発機はARM64だがx64エミュレーションでビルド |
| MFCリンク方式 | 共有DLL | skills/03-cpp-mfc の規約に従う |
| 実行環境 | Windows 10/11 | |
| バージョン管理 | Git / GitHub | |

---

## ディレクトリ構成（ソースコード）

```
claude_puls/
├── ClaudePulse.sln              … VS2022 ソリューション
└── ClaudePulse/
    ├── ClaudePulse.vcxproj      … プロジェクト（x64 / Unicode / MFC共有DLL）
    ├── ClaudePulse.vcxproj.filters
    ├── pch.h / pch.cpp          … プリコンパイル済みヘッダ
    ├── framework.h              … MFC 共通インクルード
    ├── targetver.h              … 対象 Windows バージョン
    ├── Resource.h               … リソースID定義
    ├── ClaudePulse.rc           … ダイアログ・バージョンリソース（ASCIIのみ）
    ├── ClaudePulse.h / .cpp     … CWinApp 派生（アプリ本体）
    ├── ClaudePulseDlg.h / .cpp  … メインダイアログ（1秒タイマーで更新）
    ├── PulseMeterCtrl.h / .cpp  … 心電図風パルスメータ（CStatic派生・自前描画）
    ├── UsageMonitor.h / .cpp    … JSONL 監視・使用量集計
    ├── UsageLimitsClient.h/.cpp … プラン使用制限の取得（WinHTTP・ADR-006）
    ├── UsageBarCtrl.h / .cpp    … 使用率プログレスバー（CStatic派生・自前描画）
    ├── RadioDotCtrl.h / .cpp    … 最前面表示切替の円形ラジオボタン（CStatic派生・自前描画。ADR-007）
    └── JsonLite.h               … 軽量JSON値抽出ヘルパー（共通）
```

## 命名規則

| 対象 | 規則 | 例 |
|------|------|----|
| ファイル名 | パスカルケース | `UsageMonitor.cpp` |
| クラス名 | `C` + パスカルケース（MFC慣習） | `CUsageMonitor` |
| 関数名 | パスカルケース | `FindLatestJsonl()` |
| メンバ変数 | `m_` + キャメルケース | `m_currentFile` |
| リソースID | `IDD_` / `IDC_` / `IDR_` + 大文字スネーク | `IDC_PULSE_METER` |

---

## コーディング規約

- skills/03-cpp-mfc/SKILL.md の規約に従う（`_T()` / `L""` 統一、DDXの向き、ID命名など）
- コメントは日本語で記述する
- `.rc` ファイルは ASCII のみとし、日本語UI文字列は `OnInitDialog` 等でコードから設定する（rc.exe の文字コード問題を回避）
- JSONL の解析は外部ライブラリを使わず、キー文字列検索による軽量抽出で行う

---

## データフロー

```
%USERPROFILE%\.claude\projects\**\*.jsonl   （Claude Code が書き出すセッション記録）
        │  1秒タイマー（WM_TIMER）
        ▼
CUsageMonitor::Poll()
        │  最新更新の JSONL を特定 → 前回読み取り位置から差分読み（追尾）
        │  assistant 行から "model" と "usage" 各トークン数を抽出・累計
        ▼
UsageStats（モデル名 / input / output / cache_read / cache_creation / 増分）
        │
        ├─▶ CClaudePulseDlg   … モデル名・各トークン数ラベルを更新
        └─▶ CPulseMeterCtrl   … 増分トークンを正規化しサンプル追加 → 波形再描画
```

---

## 外部依存・連携

| サービス／ファイル | 用途 | 備考 |
|--------------------|------|------|
| `%USERPROFILE%\.claude\projects\**\*.jsonl` | モデル名・使用量の取得元 | 読み取り専用 |
| `%USERPROFILE%\.claude\.credentials.json` | OAuth アクセストークン・プラン種別 | 読み取り専用（Claude Code が管理） |
| `GET https://api.anthropic.com/api/oauth/usage` | プラン使用制限（使用率%・リセット時刻） | WinHTTP・60秒間隔・ワーカースレッド（ADR-006） |
