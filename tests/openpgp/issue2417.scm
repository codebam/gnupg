#!/usr/bin/env gpgscm

;; Copyright (C) 2016 g10 Code GmbH
;;
;; This file is part of GnuPG.
;;
;; GnuPG is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 3 of the License, or
;; (at your option) any later version.
;;
;; GnuPG is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program; if not, see <http://www.gnu.org/licenses/>.

(load (with-path "defs.scm"))

(define old-home (getenv "GNUPGHOME"))

(define (touch file-name)
  (close (open file-name (logior O_WRONLY O_BINARY O_CREAT) #o600)))

(info "Checking robustness wrt empty databases in gnupghome (issue2417)...")

(lettmp
 ;; Prepare some random key to import later.
 (keyfile)
 (pipe:do
  (pipe:gpg '(--export alpha))
  (pipe:write-to keyfile (logior O_WRONLY O_BINARY O_CREAT) #o600))

 (with-temporary-working-directory
  (file-copy (path-join old-home "gpg.conf") "gpg.conf")
  (file-copy (path-join old-home "gpg-agent.conf") "gpg-agent.conf")
  (setenv "GNUPGHOME" "." #t)
  (touch "trustdb.gpg")
  (touch "pubring.gpg")
  (touch "pubring.kbx")
  (call-check `(,(tool 'GPG) --import ,keyfile))))
