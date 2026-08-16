# M5ChainOSC リリース手順

GitHub Actionsを使用して、AtomS3R用ファームウェアのビルドとGitHub Releaseのドラフト作成を自動化します。

## 初回のみ行う設定

GitHubリポジトリで`Settings` → `Actions` → `General`を開き、Actionsの利用が許可されていることを確認します。

ワークフローはRelease作成ジョブに限って`contents: write`を要求します。リポジトリやOrganizationのポリシーで書き込みが禁止されている場合は、`Workflow permissions`を確認してください。

Personal Access Tokenの追加は不要です。GitHub Actionsが発行する`GITHUB_TOKEN`を使用します。

## タグを作る前の準備

例えば次の正式版を`1.6.0`とする場合、同じコミットで次の内容を`1.6.0`へ揃えます。

- `src/config.h`の`APP_VERSION`
- `docs/installer/manifest.json`の`version`とファームウェアパス
- `docs/installer/index.html`のStable versionと更新履歴
- `docs/installer/README.md`の正式版表記、更新履歴、ファームウェアパス
- `docs/installer/firmware/M5ChainOSC-1.6.0-AtomS3R-merged.bin`

Web Installer用バイナリは、タグを作る前に実機で動作確認してください。

## タグを付ける前のActionsテスト

GitHubで`Actions` → `Build and draft release` → `Run workflow`を実行します。

手動実行では以下を行います。

1. バージョン表記の整合性確認
2. PlatformIOビルド
3. mergedバイナリ生成
4. SHA-256生成
5. Actions artifactへの保存

手動実行ではGitHub Releaseを作成しません。生成物はActions実行結果の`Artifacts`から30日間ダウンロードできます。

## Releaseのドラフトを作る

準備した変更を`main`へ反映し、ローカルが最新であることを確認します。

```powershell
git switch main
git pull
git status
```

作業ツリーがクリーンであることを確認してから、バージョンに対応するタグを作成します。

```powershell
git tag -a v1.6.0 -m "M5ChainOSC v1.6.0"
git push origin v1.6.0
```

`vX.Y.Z`形式のタグがpushされると、手動テストと同じビルドを行った後、次を添付したドラフトReleaseを作成します。

- `M5ChainOSC-X.Y.Z-AtomS3R-merged.bin`
- `M5ChainOSC-X.Y.Z-AtomS3R-SHA256.txt`

Release notesは、前回のRelease以降の変更からGitHubが自動生成します。

## 公開前の確認

GitHubのドラフトReleaseで次を確認します。

- TagとReleaseタイトルのバージョンが正しい
- Actionsが成功している
- mergedバイナリとSHA-256が添付されている
- Release notesに不足や誤りがない
- Web Installer用ファームウェアを実機で検証済み

問題がなければ`Publish release`を押します。

## バージョン不一致時

タグとファイル内のバージョンが一致しない場合、ワークフローはReleaseを作成せずエラー終了します。

タグをまだGitHubへpushしていない場合は、ローカルタグを削除して修正できます。

```powershell
git tag -d v1.6.0
```

すでにタグをpushした場合は、失敗原因を修正した新しいコミットへ同じタグを付け直すより、状況を確認してから対応してください。公開済みReleaseのタグは移動しないでください。

