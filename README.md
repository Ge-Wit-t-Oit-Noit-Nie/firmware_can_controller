# Can Controller

De can controller word gebruikt om:

1. De CAN bus te faciliteren
2. Logging centraal op te slaan
3. Tijd te synchroniseren

## Table of Contents

- [Can Controller](#can-controller)
  - [Table of Contents](#table-of-contents)
  - [Contributing](#contributing)
  - [Prerequisites](#prerequisites)
    - [GitHub Suport](#github-suport)
    - [Markdown support](#markdown-support)
    - [CMake](#cmake)
    - [C/C++](#cc)
  - [Contributors](#contributors)
  - [Tips \& Tricks](#tips--tricks)
    - [Hoe vindt ik mijn STM32 Nuleo USB port in Windows 11 met PowerShell](#hoe-vindt-ik-mijn-stm32-nuleo-usb-port-in-windows-11-met-powershell)
    - [Debug of upload geeft een foutmelding](#debug-of-upload-geeft-een-foutmelding)
    - [Mijn VSCode can de code niet compileren](#mijn-vscode-can-de-code-niet-compileren)

## Contributing

If you want to know how to add features or bugfixes, read [CONTRIBUTING.md](CONTRIBUTING.md "Reference to the CONTRIBUTING.md").

## Prerequisites

De volgende applicaties zijn nodig:

- VSCode: 1.98.2 of hoger
- [STM32CubeCLT](https://www.st.com/en/development-tools/stm32cubeclt.html#st-get-software): >= 1.18.0
- [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html): >= 6.14.0
- [STM32 VSCode Extension](https://marketplace.visualstudio.com/items?itemName=STMicroelectronics.stm32-vscode-extension)

Verder worden de volgende extenties aangeraden voor VSCode:

### GitHub Suport

- [GitHub Pull Requests](https://marketplace.visualstudio.com/items?itemName=GitHub.vscode-pull-request-github)

### Markdown support

- [Markdown All in One](https://marketplace.visualstudio.com/items?itemName=yzhang.markdown-all-in-one)
- [Markdown Table](https://marketplace.visualstudio.com/items?itemName=TakumiI.markdowntable)
- [markdownlint](https://marketplace.visualstudio.com/items?itemName=DavidAnson.vscode-markdownlint)

### CMake

- [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)

### C/C++

- [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
- [C/C++ Extension pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack)

## Timer support

- [Source](https://embeddedthere.com/stm32-timer-tutorial-using-interrupt/#Timer_Prescaler)

**Calculations**:

```txt
Time (s)                    1
Frequency (MHz)            72
Prescaler                2048
Count period        35.156,25
```

## Contributors

<table>
<tr>
    <td align="center" style="word-wrap: break-word; width: 150.0; height: 150.0">
        <a href=https://github.com/mrBussy>
            <img src=https://avatars.githubusercontent.com/u/1843912?v=4 width="100;"  style="border-radius:50%;align-items:center;justify-content:center;overflow:hidden;padding-top:10px" alt=Rudi Middel/>
            <br />
            <sub style="font-size:14px"><b>Rudi Middel</b></sub>
        </a>
    </td>
    <td align="center" style="word-wrap: break-word; width: 150.0; height: 150.0">
        <a href=https://github.com/basebom>
            <img src=https://avatars.githubusercontent.com/u/119297631?v=4 width="100;"  style="border-radius:50%;align-items:center;justify-content:center;overflow:hidden;padding-top:10px" alt=Bas de Groof/>
            <br />
            <sub style="font-size:14px"><b>Bas de Groof</b></sub>
        </a>
    </td>
</tr>
</table>


## Tips & Tricks

### Hoe vindt ik mijn STM32 Nuleo USB port in Windows 11 met PowerShell

```ps
Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match '^USB' }
```

### Debug of upload geeft een foutmelding

![Debug settings](docs/images/set_debugger.png)

### Mijn VSCode can de code niet compileren

Controleer of the juiste instellingen voor STM32 zijn gemaakt.

![STM32 Extension settings](docs/images/stm32_extention_settings.png)
