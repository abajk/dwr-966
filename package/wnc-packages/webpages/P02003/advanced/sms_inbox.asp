<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
<title>D-Link DWR-966</title>
<meta http-equiv="Content-Language" content="en-us" />
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<link rel="stylesheet" href="../style/all.css" type="text/css" />
<script language="javascript" src="../js/gettext.js" type="text/javascript"></script>
<script language="javascript" src="../js/msg.js" type="text/javascript"></script>
<script language="Javascript" src="../js/mgmt.js" type="text/javascript"></script>
<script type="text/javascript" src="../js/jquery-1.4.2.min.js"></script>
<script type="text/javascript" src="../js/jquery.json-2.2.min.js"></script>
<script type="text/javascript" src="../js/j_common.js"></script>
<script type="text/javascript" src="../js/j_sms.js"></script>
    
<script language="javascript" type="text/javascript">
CheckInitLoginInfo(<%getuser_login();%>);

var nat_enable=<%getFwNatEnable();%>;
if(nat_enable!="1")
{
	window.location.href="../login.asp";
} 
function getPINPUKCfg()
{
  var PinCfg = <%getPinConfig();%>;
  var pinArr = PinCfg.split("#");
  document.getElementById("PIN_VAR").value = pinArr[0];    
  if (pinArr[0] == "1") {          //PIN not verified
      //document.getElementById("srclte").setAttribute("src", "../images/menu/top_menu_internet_disabled.png");
      document.getElementById("hreflte").removeAttribute("href");
  }else{
      //document.getElementById("srclte").setAttribute("src", "../images/menu/top_menu_internet.png");
      document.getElementById("hreflte").href="../internet/internet_radio.asp";
  }  
  return;
}
</script>

</head>

<body>
<input type="hidden" value="<%getLanguage();%>" id="select_language" />
<div id="all">
<input type="hidden" id="PIN_VAR">
<!-- Main Menu and Submenu Start -->
<%writeMainMenu();%>
<%writeLeftMenu("adv");%>
<script type="text/javascript">menuChange("adv_menu");leftSubMenuChange("sms_submenu","sms_submenu_focus","sms_inbox","sms_inbox_href");</script>
<!-- Main Menu and Submenu End -->


<!-- Right Content start -->
<div class="contentBg">
    <div class="secH1"><script>document.write(gettext("SMS Inbox"));</script></div>
	<div class="secBg">
    
	<div id="get_sms_inbox_info">  
	  <input type="button" value="Delete All" class="tblbtn" id="inbox_delete_all" name="inbox_delete_all" />
	  <span id="short_inbox_total_num"></span>/100
	  	 
	  <table style="table-layout: fixed;" class="specTbl" border="0" cellpadding="0" cellspacing="0">
	    <tr>
	      <td class="tdH" width="16"></th>
	      <td class="tdH" width="128"><script>document.write(gettext("From / Time"));</script></th>
	      <td class="tdH" width="248"><script>document.write(gettext("Content"));</script></th>
	      <!--<td class="tdH" width="60"><script>document.write(gettext("Action"));</script></th>-->      
	      <td class="tdH" width="128" style="border-right:none;"></th>      
	    </tr>
	    <tbody id="sms_inbox_table">
	    </tbody>      
	  </table>
	</div>	  
  
  </div><!-- secBg end -->

</div><!-- contentBg end -->

</div> <!-- all end -->

<!-- sms received -->
<div id="application_short_inbox_mask" class="mask"></div>
<div class="pop_outline" id="application_short_inbox_popup_add" style="display:none; margin-top:-219px;">
  
  <div class="popup">
  <h3><script>document.write( gettext("short message details") );</script></span></td></h3>
  <div class="popup_close"><a onclick="this.className = application_inbox_popup_close();"></a></div>
  <div style="padding: 30px 40px;">
  <table width="100%" border="0" cellpadding="0">
    <tr>
      <td class="popup_text" style="margin-bottom:0; line-height:30px;"><script>document.write( gettext("from") );</script>&nbsp;:</td>
      <td class="Text_gray" id="inbox_received_from" ></td>
    </tr>
    <tr>
      <td class="popup_text" style="margin-bottom:0; line-height:30px;"><script>document.write( gettext("received time") );</script>&nbsp;:</td>
      <td class="Text_gray" id="inbox_received_time" ></td>
    </tr>
    <tr>
      <td colspan="2" class="popup_text" style="padding-top:15px;"><script>document.write( gettext("content") );</script>&nbsp;:</td>
    </tr>
    <tr>
      <td colspan="2">
      <textarea class="popup_box" name="inbox_received_content" id="inbox_received_content" style="height:100px; resize:none; width:400px;" readonly="readonly"></textarea></td>
    </tr>
    <tr>
      <td colspan="2" height="30">
       <input type="button" id="inbox_received_delete" value="delete" class="grayBN" onmousedown="this.className = 'grayBNHover'" onclick="this.className = 'grayBN'"/></td>
    </tr>
    <tr>
      <td colspan="2">
        <hr />
        <input type="button" id="inbox_received_previous" value="< previous" class="submit" />
        <input type="button" id="inbox_received_next" value="next >" class="submit" />
      </td>
    </tr>
  </table>
  </div>
  </div>
</div>
<!--sms received End-->

<!-- Apply Progress -->
<div id="progressMask" class="progressMask"></div>
<div id="progressImg" class="progressImg" >
  <img src="/images/common/progress.gif" alt="Loading..." />
</div>
  
</body>
<script type="text/javascript">
document.getElementById('inbox_delete_all').value=gettext("Delete All");	
document.getElementById('inbox_received_delete').value=gettext("delete");
document.getElementById('inbox_received_previous').value=gettext("< previous");
document.getElementById('inbox_received_next').value=gettext("next >");

</script>
</html>

