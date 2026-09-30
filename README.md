<div align="center">
  <a href="https://de4321.github.io/darkeye-webpage/" target="_blank">
    <img src="https://raw.githubusercontent.com/de4321/darkeye/main/resources/icons/logo.svg" alt="DarkEye" width="128" />
  </a>
  <h1>DarkEye</h1>
  <p><strong>ローカル資料管理を、より明確で整然と</strong></p>
  <p>完全ローカル・プライバシー重視のメディアメタデータ／個人資料管理ツール。ブラウザ拡張による補助取得と実物風 DVD ボックスの陳列に対応。整理・検索・分析・可視化をひとつに。</p>
  <br />

[![README · 日本語][badge-readme-ja]](README.md)
[![README · 简体中文][badge-readme-zh-CN]](README.CN.md)
[![README · 繁體中文][badge-readme-zh-TW]](README.zh-TW.md)

![Python][badge-python]
![Framework][badge-framework]
![Platform][badge-platform]
![License][badge-license]
![GitHub last commit][badge-last-commit]
![GitHub release][badge-release]
![GitHub Repo stars][badge-stars]
![GitHub all releases][badge-downloads]

<br />

[📖 オンラインドキュメント][link-docs]
[🎥 動画紹介][link-video]
[🌐 公式サイト][link-website]
[💬 Discord][link-discord]

</div>

<p align="center">
  <a href="#download">ダウンロードと利用</a> •
  <a href="#compliance">適法利用に関する声明</a> •
  <a href="#features">機能</a> •
  <a href="#screenshots">画面プレビュー</a> •
  <a href="#privacy">プライバシーとデータ</a> •
  <a href="#migration">移行とインポート</a> •
  <a href="#crawler">スクレイピング</a> •
  <a href="#development">開発と技術</a> •
  <a href="#community">コミュニティ</a> •
  <a href="#references">参考プロジェクト</a>
</p>

<div align="center">
  <a href="https://github.com/de4321/darkeye/releases" target="_blank">
    <img src="./docs/assets/show.jpg" alt="DarkEye 実物風 DVD の展示" width="100%" />
  </a>
</div>


---

<a id="download"></a>

## ダウンロードと利用

<div align="center">
  <a href="https://github.com/de4321/darkeye/releases/download/v1.2.5/DarkEye-v1.2.5.zip">
    <img src="https://img.shields.io/badge/%E3%83%80%E3%82%A6%E3%83%B3%E3%83%AD%E3%83%BC%E3%83%89-Windows-blue?style=for-the-badge&logo=windows" alt="Windows 版をダウンロード" />
  </a>
  　　
  <a href="https://darkeye.win/DarkEye-v1.2.5.zip">
    <img src="https://img.shields.io/badge/%E4%BB%A3%E6%9B%BF%E3%83%80%E3%82%A6%E3%83%B3%E3%83%AD%E3%83%BC%E3%83%89-WINDOWS-green?style=for-the-badge&logo=windows" alt="代替ダウンロード Windows" />
  </a>
</div>

ZIP を展開し、`exe` を実行すれば利用できます。ブラウザ拡張機能はソフトの `extensions` フォルダに同梱されており、通常は拡張を別途ダウンロードする必要はありません。スクレイピングを使う場合は、**お使いのブラウザ用を 1 つ**選び、下のドキュメントに従って拡張をインストールしてください。

### 拡張機能のインストール

👉 [オンラインドキュメント：インストール](https://de4321.github.io/darkeye/usage/#_2)

拡張単体の更新がない限り、通常は**拡張だけを別途ダウンロードする必要はありません**。
<div align="center">
  <a href="https://github.com/de4321/darkeye/releases/download/v1.2.5/chrome_capture.zip">
    <img src="https://img.shields.io/badge/%E3%83%80%E3%82%A6%E3%83%B3%E3%83%AD%E3%83%BC%E3%83%89-Chrome%2FEdge%20%E6%8B%A1%E5%BC%B5-blue?style=for-the-badge" alt="Chrome / Edge 拡張をダウンロード" />
  </a>
  　　
  <a href="https://github.com/de4321/darkeye/releases/download/v1.2.5/firefox_capture.zip">
    <img src="https://img.shields.io/badge/%E3%83%80%E3%82%A6%E3%83%B3%E3%83%AD%E3%83%BC%E3%83%89-Firefox%20%E6%8B%A1%E5%BC%B5-blue?style=for-the-badge" alt="Firefox 拡張をダウンロード" />
  </a>
</div>

### 使い方

👉 [オンラインドキュメント：使い方](https://de4321.github.io/darkeye/usage/#_3)

### バージョンと更新

👉 [FAQ：更新と移行](https://de4321.github.io/darkeye/faq/)

設定から**本体**の自動更新が可能です。ブラウザ拡張はストア公開できませんが、**拡張ファイル**はソフトの `extensions` フォルダで更新されるため、ブラウザ側で**再読み込み**が必要です。拡張は [Releases][link-releases] から手動でダウンロードも可能です。

バージョン移行時は**ブラウザ拡張の更新**にご注意ください。クローラーはサイト側の都合ですぐ失効することがあり、フィードバックを受けて手作業で保守します。プロキシの問題はソフト側では解決しません。対象サイトがブラウザで開ければ、通常は取得できます。

---

<a id="compliance"></a>

## 適法利用に関する声明

- 本ツールは、利用者が法令に基づき保有または適法に処理可能なデータおよびメタ情報の管理にのみ使用してください。
- 利用にあたっては、各国の現行法令および関連規定を遵守してください。
- 違法な取得、権利侵害となる配布、サイトのアクセス制御回避、無断での他者データ処理などへの利用を禁止します。
- サードパーティサイトのコンテンツ、API、アクセスルールは各プラットフォームの利用規約に従い、利用者自身で権限範囲を確認のうえ責任を負ってください。

---

<a id="features"></a>

## 機能

### 実装済み

| **機能** | **説明** | **状態** |
| -------- | -------- | -------- |
| **データ管理** | メディア項目・人物・タグの手動追加と CRUD | ✅ |
| **個人記録** | カスタム記録項目の手動追加と CRUD | ✅ |
| **分析・チャート** | 分析チャート・データ表示（一部未完了） | ✅ |
| **実物風 DVD ボックス陳列** | 実物風 DVD の陳列とコレクション体験 | ✅ |
| **絞り込み・フィルター表示** | 作品の絞り込み画面 | ✅ |
| **ブラウザ拡張** | Chrome / Edge / Firefox 用拡張、没入型の取得、複数サイトでの対話的取得 | ✅ |
| **簡易スクレイピング** | 取得可否と品質は対象サイトの公開方針とアクセスルールに依存。詳細はドキュメント参照 | ✅ |
| **関連グラフ** | 関連の表示、約 1 万ノードで約 60 fps | ✅ |
| **翻訳** | LLM 翻訳・ワンクリックで上書き翻訳 | ✅ |
| **mdcz NFO インポート** | [mdcz](https://github.com/ShotHeadman/mdcz) スクレイピング NFO のインポート | ✅ |
| **Jvedio NFO インポート** | Jvedio からエクスポートした NFO（試験中） | ✅ |
| **外部リンク** | JSON 駆動で外部サイトへジャンプ、カスタム可 | ✅ |
| **ローカル動画リンク** | ローカルに動画がある場合、DB にリンクできる | ✅ |
| **バックアップ** | バックアップ機能、ローカル資料の保全と復元に利用 | ✅ |
| **テーマ** | テーマ切替（3D シーンはライト／ダーク未完全対応） | ✅ |
| **スクリーンショット** | 一部スクリーンショット、女優画面で C キー | ✅ |
| **自動更新** | 更新の自動検知とダウンロード | ✅ |


### 計画・進行中

| **機能** | **説明** | **状態** |
| -------- | -------- | -------- |
| **NFO エクスポート** | 仕様の合意後に実装。ツールごとに実装差があり、データ項目も未整備 | 🔄 |

長期ロードマップや細目は [**更新履歴**](docs/CHANGELOG.md) を参照（開発に合わせて更新され、確定スケジュールではありません）。方向の抜粋：

- **AI / ツール連携**：CLI や対話などの検討（CHANGELOG の `3.x` ロードマップ）。
- **同期・共有**：WebDAV、多端バックアップ、UGC 的な情報連携など（`2.x`）。
- **体験と基盤**：タグやグラフ、UI・エクスポート、クローラーと DB などの継続改善（`1.x`）。

---

<a id="migration"></a>

## 移行とインポート

### mdcz プロジェクトの NFO

[mdcz](https://github.com/ShotHeadman/mdcz) が出力した NFO のインポートに対応しています。

👉 [ドキュメント：mdcz NFO](https://de4321.github.io/darkeye/usage/#mdcz-nfo)

### Jvedio からのデータ移行

👉 [ドキュメント：Jvedio](https://de4321.github.io/darkeye/usage/#jvedio)

---

<a id="privacy"></a>

## プライバシーとデータ

- **データと通信**：既定では実行ファイル近くの `data/` に保存（DB・設定・カバー・ポートレートなど）。ローカル資料を第三者へ自動送信することはありません。通信は主にスクレイピングとリソース取得、任意の更新ダウンロード（Cloudflare R2）、翻訳（Google または設定した LLM API）などで発生します。サードパーティサービスは利用者が任意で有効化し、適法性の確認責任を負います。


---

<a id="screenshots"></a>

## 画面プレビュー

### 実物風 DVD

![コレクション](docs/assets/dvd.jpg)

![展開](docs/assets/dvd2.jpg)

![女優](docs/assets/actress.jpg)

### フォースレイアウト・グラフ

![フォースレイアウト・グラフ](docs/assets/directforceview.jpg)

### 分析チャート

![チャート](docs/assets/chart.jpg)

### 複数作品（瀑布流）

![複数作品](docs/assets/mutiwork.jpg)

### 編集画面

![編集画面](docs/assets/edit.jpg)

### ブラウザ拡張（サイト例）

拡張を開くとローカルアプリと連携し、「追加」をクリックするとクローラーが動いてローカルに取り込みます。画面上の「お気に入り／収録」などの機能は、本機のソフトと接続しているときのみ利用できます。

![ブラウザ拡張の連携例](docs/assets/capture.JPG)

---

<a id="crawler"></a>

## スクレイピングについて

現在のスクレイピングでは、作品について公開日、監督、中日タイトルとあらすじ、女優・男優（該当時）、タグ、カバー、再生時間、メーカー、レーベル、シリーズ、スチルなどの取得を試みます。

女優情報は、アイコン、生年月日、デビュー日、スリーサイズ、身長・カップ、旧名などを主に取得します（旧名の更新ルートは未実装のため、初回に旧名で登録すると不整合が発生する場合があります）。

初回取得時には、対象サイトのアクセス方針により検証や制限が表示される場合があります。継続可否はサイト規則と利用者のアクセス権限に依存します。

現在は複数の公開データサイトに対応しています。実際に利用可能なサイトは、バージョンと対象サイト側の方針により変動するため、最新のオンラインドキュメントを参照してください。

---

<a id="development"></a>

## 開発と技術

主な技術は PySide6 / Qt Quick 3D、SQLite、ローカル FastAPI とブラウザ拡張の協調、および C++ による力指向グラフの高速化です。

👉 [開発ドキュメント](https://de4321.github.io/darkeye/development/)

---

<a id="community"></a>

## コミュニティ

質問やアイデアは Discord へ：[参加する][link-discord]

- **サポート**：ドキュメントで分からない点はお気軽に。内容は順次更新しています。
- **進捗**：新機能・開発状況・プレリリースはまず Discord で共有します。
- **参加**：ロードマップに影響したい方も、ぜひ議論に参加してください。

---

<a id="references"></a>

## 参考プロジェクト

- [mdcz](https://github.com/ShotHeadman/mdcz)：ローカル動画のファイル名から品番を取り、NFO との整合の参考
- [Jvedio](https://github.com/hitchao/Jvedio)：データベース連携・データのエクスポート
- [JavSP](https://github.com/Yuukiy/JavSP)：一部サイトのクローラー実装の参考
- [JAV-JHS](https://sleazyfork.org/zh-CN/scripts/558525-jav-jhs)：サイト情報整理の参考
- [JAV_MovieManager](https://github.com/4evergaeul/JAV_MovieManager)
- [stash](https://github.com/stashapp/stash)
- [AMMDS](https://github.com/QYG2297248353/AMMDS-Docker)
- [mdc-ng](https://github.com/mdc-ng/mdc-ng)

---

<a id="license"></a>

## ライセンス

本プロジェクトは [GNU General Public License v3.0](LICENSE) の下で提供されます。

---

## コントリビューター

<a href="https://github.com/de4321/darkeye/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=de4321/darkeye" alt="Contributors" width="500" />
</a>

---

<div align="center" style="color: gray;">DarkEye — ローカル資料の本棚を、安全に管理。</div>

<!-- Badge images -->

[badge-readme-zh-CN]: https://img.shields.io/badge/README%20%C2%B7%20%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87-555555?style=for-the-badge
[badge-readme-zh-TW]: https://img.shields.io/badge/README%20%C2%B7%20%E7%B9%81%E9%AB%94%E4%B8%AD%E6%96%87-555555?style=for-the-badge
[badge-readme-ja]: https://img.shields.io/badge/README%20%C2%B7%20%E6%97%A5%E6%9C%AC%E8%AA%9E-2ea44f?style=for-the-badge
[badge-python]: https://img.shields.io/badge/Python-3.13-blue.svg
[badge-framework]: https://img.shields.io/badge/framework-PySide6%20(Qt6)-orange
[badge-platform]: https://img.shields.io/badge/Platform-Windows-blue
[badge-license]: https://img.shields.io/github/license/de4321/darkeye
[badge-last-commit]: https://img.shields.io/github/last-commit/de4321/darkeye
[badge-release]: https://img.shields.io/github/v/release/de4321/darkeye
[badge-stars]: https://img.shields.io/github/stars/de4321/darkeye?style=social
[badge-downloads]: https://img.shields.io/github/downloads/de4321/darkeye/total

<!-- Links -->

[link-docs]: https://de4321.github.io/darkeye/
[link-video]: https://youtu.be/VCsw1D0ccgY?si=e9typx4kPnzaVFZq
[link-website]: https://de4321.github.io/darkeye-webpage/
[link-discord]: https://discord.gg/3thnEguWUk
[link-releases]: https://github.com/de4321/darkeye/releases
