/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';


var vid2 = document.getElementById('vid2');
var btn1 = document.getElementById('btn1');
var btn3 = document.getElementById('btn3');

btn1.addEventListener('click', start);

btn3.addEventListener('click', stop);

btn1.disabled = false;
btn3.disabled = true;


var pc2 = null;
var conn = null;

function start() {
  btn1.disabled = true;  
  btn3.disabled = false;
  trace('Starting Call');
  
  var servers = null;  
  pc2 = new RTCPeerConnection(servers);
  trace('Created remote peer connection object pc2');
  pc2.onicecandidate = iceCallback2;
  pc2.onaddstream = gotRemoteStream;
  
  if (window["WebSocket"]) 
  {
    		//var addr = "ws://" + location.host + ":9002" + "/wsasd";
    var addr = "ws://" + "127.0.0.1" + ":3000";
        conn = new WebSocket(addr);
        conn.onclose = function(evt) 
        {
            trace('websocket close')
        }
        conn.onmessage = function(evt) 
        {        		
            gotoffer(evt.data)
        }
  }
  else 
  {
       alert('Your browser does not support WebSocket')
  }
    
}

function onCreateSessionDescriptionError(error) {
  trace('Failed to create session description: ' + error.toString());
  stop();
}

function onCreateAnswerError(error) {
  trace('Failed to set createAnswer: ' + error.toString());
  stop();
}

function onSetLocalDescriptionError(error) {
  trace('Failed to set setLocalDescription: ' + error.toString());
  stop();
}

function onSetLocalDescriptionSuccess() {
  trace('localDescription success.');
}

function gotoffer(offer) {
  
  trace('Offer from server \n' + offer);
  //??????offer sdp????????RTCSessionDescription????
  var desc = new RTCSessionDescription();
  desc.sdp = offer;
  desc.type = 'offer';
  pc2.setRemoteDescription(desc);
  // Since the 'remote' side has no media stream we need
  // to pass in the right constraints in order for it to
  // accept the incoming offer of audio and video.
  pc2.createAnswer().then(
    gotDescription2,
    onCreateSessionDescriptionError
  );
}

function gotDescription2(desc) {
  // Provisional answer, set a=inactive & set sdp type to pranswer.
  /*desc.sdp = desc.sdp.replace(/a=recvonly/g, 'a=inactive');
  desc.type = 'pranswer';*/
  
  pc2.setLocalDescription(desc).then(
    onSetLocalDescriptionSuccess,
    onSetLocalDescriptionError
  );
  trace('Pranswer from pc2 \n' + desc.sdp);
  
  
  conn.send(JSON.stringify(desc));
  // send desc.sdp to server
}

function gotDescription3(desc) {
  // Final answer, setting a=recvonly & sdp type to answer.
  desc.sdp = desc.sdp.replace(/a=inactive/g, 'a=recvonly');
  desc.type = 'answer';
  pc2.setLocalDescription(desc).then(
    onSetLocalDescriptionSuccess,
    onSetLocalDescriptionError
  );
  trace('Answer from pc2 \n' + desc.sdp);
  pc1.setRemoteDescription(desc);
}


function stop() {
  trace('Ending Call' + '\n\n');  
  pc2.close();  
  pc2 = null;  
}

function gotRemoteStream(e) {
  vid2.srcObject = e.stream;
  trace('Received remote stream');
}

function iceCallback2(event) {
  if (event.candidate) {    
    trace('Remote ICE candidate: \n ' + event.candidate.candidate);    
    conn.send(JSON.stringify(event.candidate));
  }
  else {
    // All ICE candidates have been sent
  }
}
