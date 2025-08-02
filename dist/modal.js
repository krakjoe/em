/*
  +----------------------------------------------------------------------+
  | em                                                                   |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2025                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */
class Modal {

    constructor(id = 'modal') {
        this.element =
            document.getElementById(id);
        this.title = new Modal.Title(this);
        this.title = new Modal.Title(this);
        this.info  = new Modal.Info(this);
        this.input = new Modal.Input(this);
        this.accept = new Modal.Button(this, "#modal-accept");
        this.reject = new Modal.Button(this, "#modal-reject");
    }

    show(title, info, input, accept, reject) {
        return new Promise((onResolve, onReject) => {
            this.title.show(title);
            this.info.show(info);
            this.input.show(input);
            this.accept.show(
                accept.text, () => {
                    if (accept.handler) {
                        accept.handler();
                    }
                    onResolve(this.result());
                    this.hide();
                });
            this.reject.show(
                reject.text, () => {
                    if (reject.handler) {
                        reject.handler();
                    }
                    onReject();
                    this.hide();
                });
            this.element.style.display = "block";
        });
    }

    hide() {
        this.element.style.display = "none";
        this.element.removeEventListener(
            "keydown", this.keydown);
        this.title.hide();
        this.info.hide();
        this.input.hide();
        this.accept.hide();
        this.reject.hide();
    }

    result() {
        return this.input.result();
    }
}

Modal.Title = class {
    constructor(modal) {
        this.modal = modal;
        this.element = this.modal.element
            .querySelector("#modal-title");
    }

    show(text) {
        this.element.textContent = text;
        this.element.style.display = "block";
    }

    hide() {
        this.element.style.display = "none";
        this.element.textContent = null;
    }
}

Modal.Input = class {
    constructor(modal) {
        this.modal   = modal;
        this.element = this.modal.element
            .querySelector("#modal-input");
        this.element.addEventListener(
            "keydown",
            this.keydown.bind(this));
    }

    show(text) {
        if (text && text.length) {
            this.element.value = text;
        } else {
            this.element.value = null;
        }
        
        this.element.style.display = "block";
    }

    hide() {
        this.element.style.display = "none";
        this.element.textContent = null;
    }

    result() {
        return this.element.value;
    }

    keydown(event) {
        if (event.key === 'Escape') {
            this.modal.reject.event(event);
        } else if (event.key === 'Enter') {
            this.modal.accept.event(event);
        }
    }
}

Modal.Info = class {
    constructor(modal) {
        this.modal   = modal;
        this.element = this.modal.element
            .querySelector("#modal-info");
    }

    show(text) {
        if (text && text.length) {
            this.element.textContent = text;
            this.element.style.display = "block";
            return;
        }

        this.element.textContent = null;
        this.element.style.display = "none";
    }

    hide() {
        this.element.style.display = "none";
    }
}

Modal.Button = class {
    constructor(modal, selector) {
        this.modal   = modal;
        this.element = this.modal.element
            .querySelector(selector);
        this.element.addEventListener(
            "click", this.event.bind(this));
    }

    show(text, action) {
        this.action = action.bind(this);
        this.element.textContent   = text;
        this.element.style.display = "block";
    }

    hide() {
        this.action = null;
        this.element.style.display = "none";
    }

    event(event) {
        if (this.action) {
            this.action();
        }
    }
}
