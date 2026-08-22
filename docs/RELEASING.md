# M5ChainOSC リリース手順

GitHub Actionsを使用して、AtomS3R用ファームウェアのビルドとGitHub Releaseのドラフト作成を自動化します。

## 初回のみ行う設定

GitHubリポジトリで`Settings` → `Actions` → `General`を開き、Actionsの利用が許可されていることを確認します。

ワークフローはRelease作成ジョブに限って`contents: write`を要求します。リポジトリやOrganizationのポリシーで書き込みが禁止されている場合は、`Workflow permissions`を確認してください。

Personal Access Tokenの追加は不要です。GitHub Actionsが発行する`GITHUB_TOKEN`を使用します。

GitHubリポジトリの`Settings` → `Pages` → `Build and deployment`を開き、`Source`を`GitHub Actions`へ変更します。Pages配信ジョブは`pages: write`と`id-token: write`を使用します。

## タグを作る前の準備

例えば次の正式版を`1.7.0`とする場合、タグを作る前のコミットで次を更新します。

- `src/config.h`の`APP_VERSION`
- `docs/installer/manifest.json`のバージョンとファームウェアパス
- `docs/installer/index.html`のStable versionと更新履歴
- `docs/installer/README.md`の正式版表記と更新履歴

mergedバイナリを`docs/installer/firmware/`へ手作業で配置する必要はありません。Release公開後、Pages配信WorkflowがRelease Assetから自動取得します。

## タグを付ける前のActionsテスト

GitHubで`Actions` → `Build and draft release` → `Run workflow`を実行します。

手動実行では以下を行います。

1. `APP_VERSION`の形式確認
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
git tag -a v1.7.0 -m "M5ChainOSC v1.7.0"
git push origin v1.7.0
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

問題がなければ`Publish release`を押します。Releaseの公開を検知すると、`Deploy GitHub Pages` Workflowが次を自動実行します。

1. 公開されたReleaseから対応するmergedバイナリをダウンロード
2. `manifest.json`、バージョン表記、ファームウェアファイルの整合性を確認
3. JekyllでドキュメントとWeb Installerをビルド
4. GitHub Pagesへ配信

配信完了後、Actionsの`Deploy GitHub Pages`と公開中のWeb Installerを確認してください。

## ドキュメントだけを更新する

`docs/`以下だけを`main`へpushした場合もPages配信Workflowが実行されます。`manifest.json`に対応する公開済みRelease Assetを取得し直すため、ファームウェアをリポジトリへコミットする必要はありません。

manifestのファームウェアパスは、次の形式にします。

```text
firmware/M5ChainOSC-1.7.0-AtomS3R-merged.bin
```

ローカルに対応するファームウェアを配置した場合は、Installerの完全な整合性を確認できます。

```powershell
python scripts/check_release_version.py --check-installer
```

ファームウェアを配置しない状態でメタデータだけを確認する場合は、次を実行します。

```powershell
python scripts/check_release_version.py --check-installer-metadata
```

## バージョン不一致時

タグ、`APP_VERSION`、Web Installerのバージョン表記が一致しない場合、ワークフローはReleaseを作成せずエラー終了します。

タグをまだGitHubへpushしていない場合は、ローカルタグを削除して修正できます。

```powershell
git tag -d v1.7.0
```

すでにタグをpushした場合は、失敗原因を修正した新しいコミットへ同じタグを付け直すより、状況を確認してから対応してください。公開済みReleaseのタグは移動しないでください。
