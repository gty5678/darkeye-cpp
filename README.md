<div align="center">
  <h1>DarkEye</h1>
  <p><strong>Insight and order</strong></p>
  <p>A fully local personal media library, metadata editor, relationship analyzer, and archive browser.</p>
  <br />

[![README · English][badge-readme-en]](README.md)
[![README · 繁體中文][badge-readme-zh-TW]](README.zh-TW.md)
[![README · 日本語][badge-readme-ja]](README.ja.md)

![Python][badge-python]
![Framework][badge-framework]
![Platform][badge-platform]
![License][badge-license]
![GitHub last commit][badge-last-commit]
![GitHub release][badge-release]
![GitHub Repo stars][badge-stars]

<br />

[📖 Docs][link-docs]
[🎥 Video][link-video]
[🌐 Website][link-website]
[💬 Discord][link-discord]

</div>

<p align="center">
  <a href="#compliance">Legal and compliant use</a> •
  <a href="#download">Download and usage</a> •
  <a href="#features">Features</a> •
  <a href="#screenshots">Screenshots</a> •
  <a href="#privacy">Privacy and data</a> •
  <a href="#migration">Migration and import</a> •
  <a href="#development">Development</a> •
  <a href="#community">Community</a> •
  <a href="#references">Related projects</a>
</p>


---

<a id="compliance"></a>

## Legal and compliant use

- This tool is for managing data and metadata that you lawfully own, are authorized to handle, or may otherwise process in compliance with applicable law.
- When using this tool, follow the laws and regulations in force in your jurisdiction.
- Do not use this tool for unlawful scraping, infringing distribution, bypassing site access controls, or processing other people’s data without authorization, among other abuses.
- Third-party sites, APIs, and access rules are governed by their platform terms; you are responsible for compliance.
- Do not download third-party software indiscriminately.
- This project is a general-purpose local tool for managing data you lawfully own or are authorized to handle.
- It must not be used for infringing distribution, fraud, access-control circumvention, unauthorized scraping, or other illegal activities.
- The project will never private-message you to ask for verification codes, remote control, or money. Download only from official release channels and verify file hashes or signatures when provided.

<a id="download"></a>

## Download and usage

<div align="center">
  <a href="https://github.com/de4321/darkeye/releases/download/v1.2.5/DarkEye-v1.2.5.zip">
    <img src="https://img.shields.io/badge/Download-Windows-blue?style=for-the-badge&logo=windows" alt="Download for Windows" />
  </a>

  <a href="https://darkeye.win/DarkEye-v1.2.5.zip">
    <img src="https://img.shields.io/badge/Alternate%20Download-WINDOWS-green?style=for-the-badge&logo=windows" alt="Alternate Windows download" />
  </a>
</div>

Download, extract, and run the executable. Browser extensions are shipped with the app under the `extensions` directory. Install **one** extension for your browser, as described in the docs below.

### Browser extension

👉 [Docs: install the browser extension](https://de4321.github.io/darkeye/usage/#_2)

You usually **do not** need a separate extension download unless the extension is updated on its own.
<div align="center">
  <a href="https://github.com/de4321/darkeye/releases/download/v1.2.5/chrome_capture.zip">
    <img src="https://img.shields.io/badge/Download-Chrome%2FEdge%20Extension-blue?style=for-the-badge" alt="Download Chrome/Edge extension" />
  </a>
  　　
  <a href="https://github.com/de4321/darkeye/releases/download/v1.2.5/firefox_capture.zip">
    <img src="https://img.shields.io/badge/Download-Firefox%20Extension-blue?style=for-the-badge" alt="Download Firefox extension" />
  </a>
</div>

### User guide

👉 [Docs: usage](https://de4321.github.io/darkeye/usage/#_3)

### Versions and updates

👉 [FAQ: updates and migration](https://de4321.github.io/darkeye/faq/)

Settings can check for and download updates to the **application** itself. The app does not auto-update the **browser extension**; extensions in the `extensions` folder are updated with the app, and you need to **reload the extension in the browser** manually. You can also download extensions from [Releases][link-releases].

When migrating to a new version, **update the browser extension** as well.



---

<a id="features"></a>

## Features

### Implemented

| **Feature** | **Description** | **Status** |
| -------- | -------- | -------- |
| **Data management** | CRUD for media items, people, tags, and related data | ✅ |
| **Personal records** | Manual add and CRUD for custom record entries | ✅ |
| **Analysis and charts** | Charts and analysis views (some areas still in progress) | ✅ |
| **Skeuomorphic DVD shelf** | DVD-style display and collection experience | ✅ |
| **Filters and views** | Filtered work listing pages | ✅ |
| **Relationship graph** | Explore relationships; ~60 fps with ~10k nodes | ✅ |
| **Translation** | LLM translation and one-click overwrite | ✅ |
| **Local video links** | Link local video files into the database when present | ✅ |
| **Backups** | Backup system for local archive and restore | ✅ |
| **Theming** | Theme switching (3D scene does not fully follow light/dark yet) | ✅ |
| **Auto-update** | Check and download app updates | ✅ |
| **mdcz NFO import** | NFO import from [mdcz](https://github.com/ShotHeadman/mdcz) | ✅ |
| **Jvedio NFO import** | Jvedio export NFO (testing) | ✅ |


### Planned / in progress

| **Feature** | **Description** | **Status** |
| -------- | -------- | -------- |
| **NFO export** | After consensus; field mapping varies across tools and is not complete | 🔄 |

Long-term plans and more detail: [**Changelog and roadmap**](docs/CHANGELOG.md) (rolling; not a fixed schedule).



---

<a id="migration"></a>

## Migration and import

### mdcz NFO import

NFO import from [mdcz](https://github.com/ShotHeadman/mdcz) is supported.

👉 [Docs: mdcz NFO](https://de4321.github.io/darkeye/usage/#mdcz-nfo)

### Jvedio migration

👉 [Docs: Jvedio](https://de4321.github.io/darkeye/usage/#jvedio)

---

<a id="privacy"></a>

## Privacy and data

- **Data and networking**: By default, data lives next to the app in `data/` (database, config, covers, avatars, etc.). The app does not proactively upload your local library to third parties. Network use mainly comes from scraping, resource fetches, optional update downloads (Cloudflare R2), and translation (Google or a self-hosted LLM API you configure). You enable third-party services and are responsible for compliance.


---

<a id="screenshots"></a>

## Screenshots


![Force-directed graph](docs/assets/directforceview.jpg)

![Charts](docs/assets/chart.jpg)

---

<a id="development"></a>

## Development

The stack is PySide6 / Qt Quick 3D, SQLite, local FastAPI, browser extensions, and a C++ force-directed graph for performance.

To develop locally, follow the guide below to run the app.

👉 [Development docs](https://de4321.github.io/darkeye/development/)

---

<a id="community"></a>

## Community

Questions or ideas? Join Discord: [Join the community][link-discord]

- **New users**: Ask if something in the docs is unclear; the online docs are updated over time.
- **Early access**: New features, progress, and pre-releases are discussed on Discord first.
- **Influence the roadmap**: Come discuss what you would like to see.

---

<a id="references"></a>

## Related projects

- [mdcz](https://github.com/ShotHeadman/mdcz) (migration compatibility)
- [Jvedio](https://github.com/hitchao/Jvedio) (migration compatibility)
- [JavSP](https://github.com/Yuukiy/JavSP) (site adapter ideas)
- [JAV-JHS](https://sleazyfork.org/zh-CN/scripts/558525-jav-jhs) (information organization ideas)
- [JAV_MovieManager](https://github.com/4evergaeul/JAV_MovieManager) (media management UX)
- [stash](https://github.com/stashapp/stash)
- [AMMDS](https://github.com/QYG2297248353/AMMDS-Docker)
- [mdc-ng](https://github.com/mdc-ng/mdc-ng)

---

<a id="license"></a>

## License

This project is released under the [GNU General Public License v3.0](LICENSE).

---
## Contributors

<a href="https://github.com/de4321/darkeye/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=de4321/darkeye" alt="Contributors" width="500" />
</a>

---

<div align="center" style="color: gray;">DarkEye — a local media shelf, under your control.</div>

<!-- Badge images -->

[badge-readme-en]: https://img.shields.io/badge/README%20%C2%B7%20English-2ea44f?style=for-the-badge
[badge-readme-zh-CN]: https://img.shields.io/badge/README%20%C2%B7%20%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87-555555?style=for-the-badge
[badge-readme-zh-TW]: https://img.shields.io/badge/README%20%C2%B7%20%E7%B9%81%E9%AB%94%E4%B8%AD%E6%96%87-555555?style=for-the-badge
[badge-readme-ja]: https://img.shields.io/badge/README%20%C2%B7%20%E6%97%A5%E6%9C%AC%E8%AA%9E-555555?style=for-the-badge
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
