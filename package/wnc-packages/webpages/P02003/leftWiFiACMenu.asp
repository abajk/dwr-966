<div id="left_bg">
  <ul>
   <li class="leftbu" id="wifi_clientlist"><a href="#" onclick="RedirectSubMenu('wifi_clientlist');" id="wifi_clientlist_href"><script>document.write(gettext("Device List"));</script></a></li>
   <li class="leftbu" id="wifi_settings"><a href="#" onclick="RedirectSubMenu('wifi_settings');" id="wifi_settings_href"><script>document.write(gettext("Wi-Fi Settings "));</script></a></li>
   <li class="leftbu" id="wifi_advsettings"><a href="#" onclick="RedirectSubMenu('wifi_advsettings');" id="wifi_advsettings_href"><script>document.write(gettext("Wi-Fi Advanced "));</script></a></li>
     <dl style="display:none;" id="wifi_submenu">
       <dd class="ddbu" id="wifi_802.11n"><a href="#" onclick="RedirectSubMenu('wifi_advsettings');" id="wifi_802.11n_href"><script>document.write(gettext("Wi-Fi 2.4GHz"));</script></a></dd>
       <dd class="ddbu" id="wifi_802.11ac"><a href="#" onclick="RedirectSubMenu('wifi_advsettings_ac');" id="wifi_802.11ac_href"><script>document.write(gettext("Wi-Fi 5GHz"));</script></a></dd>
     </dl>
   <li class="leftbu" id="wifi_wps"><a href="#" onclick="RedirectSubMenu('wifi_wps');" id="wifi_wps_href">WPS</a></li>
  </ul>
</div>
