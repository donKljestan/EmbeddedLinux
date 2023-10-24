```
Secured mailbox app emulation
Cilj projektnog zadatka je razviti jednostavan sistem za zaštićenu razmenu poruka između 2 učesnika. Oba učesnika predstavljena su programskim nitima. Prenos zaštićenih podataka takođe emulira jedna programska nit.

Sistem se sastoji od:

Znakovnog uređaja (character device driver) u kernel prostoru koji obezbeđuje zaštićenu razmenu poruka, tako da:
[5 bodova] Uređaj podrži operacije čitanja i pisanja nad podacima dužine do 50 znakova
Uređaj radi u dva režima rada (omogućiti izbor npr. preko parametra mode, ioctl sistemskog poziva, itd.): 
[5 bodova] Režim enkripcije (encryption_mode) - uređaj u ovom režimu, za sve podatke koje preuzme od korisnika, računa kontrolnu sumu nad podacima i nakon toga ih enkriptuje; na zahtev korisnika vraća enkriptovane podatke i kontrolnu sumu.
[10 bodova] Režim dekripcije (decryption_mode) - uređaj u ovom režimu, za sve podatke koje preuzme od korisnika (enkriptovane podatke i kontrolnu sumu pre enkripcije), dekriptuje podatke, računa kontrolnu sumu dekriptovanih podataka i proverava podudaranje kontrolne sume sa preuzetom; u slučaju podudaranja kontrolne sume ispisuje poruku  "CRC sum match" odgovarajućeg pririteta u kernel log, u suprotnom ispisuje poruku  "CRC sum mismatch" odgovarajućeg pririteta; na zahtev korisnika vraća dekriptovane podatke.
[5 bodova] Algoritami:
Algoritam enkripcije odrediti samostalno tako da postoji inverzna operacija koja će predstavljati algoritam dekripcije. Ukoliko se koriste neki paramteri, definisati ih kao ulazni parametar modula.
Algoritam za određivanje kontrolne sume odrediti takođe samostalno.

Aplikacije u korisničkom prostoru koja se sastoji od tri niti koje međusobno razmenjuju podatke:
[5 bodova] Prva nit na svakih 2s treba da izgeneriše nasumične podatke nasumične dužine do 50 znakova i da ih prosledi drugoj niti preko znakovnog uređaja u odgovarajućem režimu i da to signalizira drugoj niti.
[10 bodova] Druga nit prihvata podatke od znakovnog uređaja u režimu enkripcije i ispisuje ih na standardni izlaz, a zatim prosleđuje na dekripciju znakovnom uređaju u odgovarajućem režimu i signalizira trećoj niti. Potrebno je omogućiti ubacivanje greške (error injection) između čitanja i upisivanja (u toku emulacije prenosa podataka).
[5 bodova] Treća nit prihvata podatke od znakovnog uređaja u režimu dekripcije i ispisuje ih na standardni izlaz.
[5 bodova] Potrebno je obezbediti:

Odgovarajuće Makefile datoteke koje će omogućiti prevođenje rukovaoca znakovnog uređaja i aplikacije
Odgovarajuće bash skripte koje će omogućiti pokretanje sistema u odgovarajućim režimima rada tako da izlazni podaci budu prosleđeni na standardni izlaz i sačuvani u izlaznu log datoteku.
Predat projekat koji nosi do 50 bodova, biće ocenjen prema stavkama koje su navedene u specifikaciji, sa posebnom pažnjom na:

Rukovalac:
Korektno ponašanje, u skladu sa specifikacijom
Korektno zauzimanje i oslobađanje resursa
Celine iz specifikacije korektno implementirane i korektno pozicionirane u odgovarajuće funkcije rukovaoca
Aplikacija:
Korektno ponašanje, u skladu sa specifikacijom
Korektno zauzimanje i oslobađanje resursa
Korektna sinhronizacija niti
Korektan završetak rada aplikacije

Dodatni zadatak (10 bodova bonus)
Proširiti znakovni uređaj tako da u režimu dekripcije nakon provere podudaranja CRC sume: 
uključuje zelenu LE diodu na RPi uređaju u slučaju podudaranja
isključuje zelenu LE diodu u slučaju nepodudaranja
```
