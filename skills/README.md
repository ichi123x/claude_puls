# skills/ — Claude Codeスキル定義フォルダ

## 概要

このフォルダには、特定タスクに特化したClaude Codeへの指示（スキル）を格納する。

Claude Codeはタスク開始時に該当スキルを読み込み、そのタスクに最適な動作をする。

---

## スキルの作り方

各スキルは `SKILL.md` という名前のMarkdownファイルで定義する。

```
skills/
├── README.md               # このファイル
├── report-generation/
│   └── SKILL.md            # レポート生成スキル
├── data-processing/
│   └── SKILL.md            # データ処理スキル
└── （タスク名）/
    └── SKILL.md
```

---

## SKILL.mdの基本フォーマット

```markdown
# スキル名

## 目的
このスキルが何をするか1〜2文で説明

## 前提条件
- 必要なファイル・環境

## 手順
1. ステップ1
2. ステップ2

## 注意事項
- 気をつけること
```

---

## 読み込み方（CLAUDE.mdへの記載例）

```
# 例：レポート生成タスクの場合
skills/report-generation/SKILL.md を読み込んでから作業を開始すること
```
