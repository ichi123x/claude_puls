# CLAUDE.md — プロジェクト指示書

## プロジェクト概要

- **プロジェクト名**：PROJ_ClaudePulse
- **目的**：Claude Code の現在使用しているモデル名と使用量をリアルタイムで表示するツール
- **担当者**：ihira
- **プラットフォーム**：C++ / MFC

---

## Claudeへの基本指示

- 応答・コメントは**日本語**で統一すること
- コードのコメントも日本語で記述すること
- 不明点があれば作業前に必ず確認すること
- 破壊的変更（ファイル削除・上書き）は事前に報告し、承認を得てから実行すること
- タスク完了時は `tasks/tasklist.md` の該当項目をチェック済みに更新すること

---

## ディレクトリ構成

- ディレクトリ（skills/.steering/tasks）がない場合は新規作成する
- ディレクトリ（skills/.steering/tasks）に該当するファイルがない場合、空で新規作成する
```
PROJ_ClaudePulse/
├── CLAUDE.md
├── skills/
│   ├── 00-common/
│   │   └── SKILL.md
│   ├── 01-cs-winforms/
│   │   └── SKILL.md
│   ├── 02-cs-wpf/
│   │   └── SKILL.md
│   ├── 03-cpp-mfc/
│   │   └── SKILL.md
│   ├── 04-cpp-win32/
│   │   └── SKILL.md
│   └── 05-web/
│       └── SKILL.md
├── .steering/
│   ├── product.md
│   ├── structure.md
│   └── decisions.md
└── tasks/
    └── tasklist.md
```

---

## 開発フロー（スペック駆動）

```
1. 要件定義   → .steering/product.md を更新
2. 設計       → .steering/structure.md を更新
3. タスク分解  → tasks/tasklist.md にタスクを追加
4. 実装       → タスクを上から順に実施（MFC作業時は上記の行動規約を厳守）
5. 確認       → タスクをチェック済みに更新
6. ビルド検証  → VS2022でビルド・動作確認、エラーはテキストで共有
```
