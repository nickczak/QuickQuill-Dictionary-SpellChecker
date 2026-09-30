import { Component, OnInit, OnDestroy, HostListener, inject } from '@angular/core';
import { Router, RouterOutlet } from '@angular/router';
import { Subscription } from 'rxjs';
import { Header } from './shared/header/header';
import { Footer } from './shared/footer/footer';
import { Auth } from './services/auth';
import { Wotd } from './services/wotd';

@Component({
  selector: 'app-root',
  imports: [RouterOutlet, Header, Footer],
  templateUrl: './app.html',
  styleUrl: './app.css',
})
export class App implements OnInit, OnDestroy {
  constructor(private router: Router) {}

  private auth = inject(Auth);
  private wotd = inject(Wotd);
  private authSub?: Subscription;
  private wotdSub?: Subscription;

  ngOnInit() {
    this.authSub = this.auth.refreshSession().subscribe();
    // Fetch the daily word at startup; the dictionary displays this payload as
    // the initial search result as soon as it arrives.
    this.wotdSub = this.wotd.fetch().subscribe();
  }

  ngOnDestroy() {
    this.authSub?.unsubscribe();
    this.wotdSub?.unsubscribe();
  }

  @HostListener('document:keydown.escape')
  onEscape() {
    this.router.navigate(['/']);
  }
}
