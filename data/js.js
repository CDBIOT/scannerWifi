var timer = 0;

function decT() {
  timer--;
  document.getElementById("time").innerHTML = timer;
  if (timer == 0) {
    window.location = "/";
  } else {
    window.setTimeout("decT()", 1000);
  }
}

function setT(t) {
  timer = t;
  decT();
}

function reqChange() {
  if (this.readyState == 4) {
    if (this.status == 200 && this.responseText != null) {
      var data = this.responseText.split("&");
      var l = (data.length - 1) / 2 - 1;
      document.getElementById("now"  ).innerText = data[0];
      for (var i = 0; i <= l; i++) {
        var r = document.getElementById("r" + i);
        if(data[1 + i * 2] == 1) {
          r.innerText = "Ligado";
          r.style.background = "rgb(170, 236, 83)";
        } else {
          r.innerText = "Desligado";
          r.style.background = "rgb(227, 0, 14)";
        }
        document.getElementById("e" + i).innerText = data[2 + i * 2];
      }
    } else {
      alert("Falha obtendo status do relé.");
    }
  }
}

var reqStatus = new XMLHttpRequest();
reqStatus.onreadystatechange = reqChange;

var reqSet = new XMLHttpRequest();
reqSet.onreadystatechange = reqChange;

function relayStatus() {
  reqStatus.open("GET", "/relayStatus", true);
  reqStatus.send(null);
  window.setTimeout("relayStatus()", 2000);
}

function setRelay(s, c) {
  var x = s[s.length - 1];
  if (x == "0") {
    x = "desligar";
  } else if (x == "1") {
    x = "ligar";
  } else {
    x = "inverter";
  }
  if (c == 0 || confirm("Confirma " + x + "?")) {
    reqSet.open("GET", "/relayStatus?set=" + s, true);
    reqSet.send(null);
  }
}

function logDownload() {
  window.location = "/logGet";
}

function logReset() {
  if (confirm('Confirma reiniciar Log?')) {
    window.location = "/logReset";
  }
}

function chkReset() {
  if (confirm("Confirma redefinir todas as Configurações?")) {
    window.location = "/configReset";
  }
}

function chkSave() {
  var webPW = document.config.webPW.value;
  if (webPW != document.config.webPWC.value) {
    alert("Atenção!\nSenhas de Acesso à Interface são diferentes.");
    return false;
  }
  if (webPW != "" && webPW.length < 6) {
    alert("Atenção!\nSenha de Acesso à Interface deve possuir pelo menos 6 caracteres.");
    return false;
  }
  cfgPW = document.config.cfgPW.value;
  if (cfgPW != document.config.cfgPWC.value) {
    alert("Atenção!\nSenhas de Redefinição da Configuração são diferentes.");
    return false;
  }
  if (cfgPW != "" && cfgPW.length < 10) {
    alert("Atenção!\nSenha de Redefinição da Configuração deve possuir pelo menos 10 caracteres.");
    return false;
  }
  if (webPW != "" && webPW == cfgPW) {
    alert("Atenção!\nAs senhas de Acesso à Interface e Redefinição da Configuração devem ser diferentes.");
    return false;
  }
  if (cfgPW != "" && !confirm("*** ATENÇÃO ***\n\nA Senha de Redefinição da Configuração está sendo alterada, registre-a em um lugar seguro.\n\nCaso esta senha seja perdida, o Dispositivo só poderá ser reiniciado na Fábrica.\n\nConfirma a alteração dessa Senha?")) {
    return false;
  }
  return confirm("Gravar a Configuração?");
}

function menu(m) {
  var s = document.getElementById(m).style;
  if (s.display == "none") {
    s.display = "";
  } else {
    s.display = "none";
  }
}
