/*
 * This file is part of the ScrewSync project.
 * Copyright (c) 2026 Donatas Olsevičius
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef wifi_portal_h
#define wifi_portal_h

static const char WEB_WIFI_HTML[] = R"WIFIHTML(<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi setup &mdash; {{DEVICE_NAME}}</title>
<style>
  :root { color-scheme: light dark; }
  body { font: 15px/1.55 system-ui, sans-serif; margin: 1.5rem; max-width: 50rem; }
  h1 { font-size: 1.3rem; }
  code { background: rgba(127,127,127,.18); padding: .05rem .3rem; border-radius: .2rem; }
  .dim { opacity: .6; }
  form label { display: block; margin: .5rem 0; }
  form input { display: block; padding: .5rem; width: 100%; max-width: 20rem; box-sizing: border-box; }
  form button { margin-top: .6rem; padding: .5rem 1rem; }
</style>

<h1>WiFi setup</h1>

<form method="post">
  <label>Network name (SSID)<input name="s" autofocus autocomplete="off"></label>
  <label>Password<input name="p" type="password" autocomplete="off"></label>
  <button>Save &amp; connect</button>
</form>

<p class="dim">The device stores this and restarts to join the network. Then reach it
at the address shown on its display, or <code>{{MDNS_HOST}}</code>. This access
point drops when it connects.</p>

<p><a href="/">&larr; back</a></p>
)WIFIHTML";

#endif
