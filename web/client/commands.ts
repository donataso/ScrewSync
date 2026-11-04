export const cmd = {
  reboot: () => "reboot",
  mode: (name: string) => `mode ${name}`,
  displayInfo: () => "display info",
  displayFlash: () => "display flash",
  wifiScan: () => "wifi scan",
  wifiUse: (ssid: string) => `wifi use ${ssid}`,
  wifiDel: (ssid: string) => `wifi del ${ssid}`,
}
