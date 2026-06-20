# Node.js + Claude Code Dev Container

Node.js と Claude Code がプリインストールされた開発コンテナのベーステンプレートです。

## 必要なもの

- [Docker Desktop](https://www.docker.com/products/docker-desktop/)
- [Visual Studio Code](https://code.visualstudio.com/)
- VS Code拡張機能: [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)

## セットアップ

### 1. コンテナを起動

1. VS Code でこのフォルダを開く
2. 右下に表示される **「Reopen in Container」** をクリック
   （または `Ctrl+Shift+P` → `Dev Containers: Reopen in Container`）
3. コンテナのビルドが完了するまで待つ（初回は数分かかります）

### 2. 動作確認

ターミナルで確認：
```bash
node --version   # Node.js のバージョン
claude --version # Claude Code のバージョン
```

## 含まれるもの

| ツール | バージョン |
|--------|-----------|
| Node.js | 20.x (LTS) |
| Claude Code | 最新版 |

## 使い方

コンテナ内のターミナルで Claude Code を起動：
```bash
claude
```

## 注意事項

- `.claude` フォルダもローカルとコンテナ間で共有されます（認証情報・設定の共有）
