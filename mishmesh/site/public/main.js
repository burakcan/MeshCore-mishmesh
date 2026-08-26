(function(){
  var root=document.documentElement, tb=document.getElementById("theme");
  function cur(){return root.getAttribute("data-theme")||(matchMedia("(prefers-color-scheme:light)").matches?"light":"dark");}
  function set(t){root.setAttribute("data-theme",t);if(tb)tb.textContent="[ "+t+" ]";}
  set(cur());
  if(tb)tb.addEventListener("click",function(){set(cur()==="dark"?"light":"dark")});

  // Central link wiring - change REPO to point elsewhere.
  var REPO="https://github.com/burakcan/MeshCore-mishmesh";
  document.querySelectorAll("[data-repo]").forEach(function(a){a.href=REPO});
  document.querySelectorAll("[data-releases]").forEach(function(a){a.href=REPO+"/releases"});
  document.querySelectorAll("[data-manual]").forEach(function(a){a.href="manual.html"});
  document.querySelectorAll("[data-index]").forEach(function(a){a.href="index.html"});

  // Hero boot reveal (landing).
  var hero=document.querySelector(".hero");
  if(hero)requestAnimationFrame(function(){hero.classList.add("revealed")});

  // Install method tabs (landing).
  document.querySelectorAll(".pill button").forEach(function(b){
    b.addEventListener("click",function(){
      document.querySelectorAll(".pill button").forEach(function(x){x.classList.remove("sel")});
      b.classList.add("sel");
      var t=b.dataset.tab;
      document.querySelectorAll("[data-panel]").forEach(function(p){p.hidden=(p.dataset.panel!==t)});
    });
  });

  // Copy buttons on code blocks (landing).
  document.querySelectorAll(".copy").forEach(function(btn){
    btn.addEventListener("click",function(){
      var code=btn.parentElement.querySelector("code").innerText;
      if(navigator.clipboard)navigator.clipboard.writeText(code);
      var o=btn.textContent;btn.textContent="copied";setTimeout(function(){btn.textContent=o},1200);
    });
  });

  // Sidebar scrollspy (manual).
  var toc=document.getElementById("toc");
  if(toc){
    var links={};
    toc.querySelectorAll("a").forEach(function(a){links[a.getAttribute("href").slice(1)]=a});
    var obs=new IntersectionObserver(function(es){
      es.forEach(function(e){
        if(e.isIntersecting){
          Object.keys(links).forEach(function(k){links[k].classList.remove("on")});
          if(links[e.target.id])links[e.target.id].classList.add("on");
        }
      });
    },{rootMargin:"-20% 0px -70% 0px"});
    document.querySelectorAll("article .sec").forEach(function(s){obs.observe(s)});
  }
})();
