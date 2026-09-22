import { Component, OnInit, OnDestroy, HostListener, inject } from '@angular/core';
import { Router, RouterOutlet } from '@angular/router';
import { Subscription, combineLatest, timer } from 'rxjs';
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
  /** Hard cap so a cold Render server can never strand the user on the splash. */
  private static readonly SPLASH_MAX_MS = 10000;
  private static readonly SPLASH_ID = 'app-splash';

  constructor(private router: Router) {}

  private auth = inject(Auth);
  private wotd = inject(Wotd);
  private bootSub?: Subscription;
  private splashHidden = false;

  ngOnInit() {
    // Keep the user's session alive across visits and fetch the word of the
    // day in parallel. Both settle before the loading splash fades out.
    this.bootSub = combineLatest([this.auth.refreshSession(), this.wotd.fetch()]).subscribe({
      next: () => this.hideSplash(),
      error: () => this.hideSplash(),
    });

    timer(App.SPLASH_MAX_MS).subscribe(() => this.hideSplash());
  }

  ngOnDestroy() {
    this.bootSub?.unsubscribe();
  }

  private hideSplash() {
    if (this.splashHidden) return;
    this.splashHidden = true;
    const el = document.getElementById(App.SPLASH_ID);
    if (!el) return;
    el.classList.add('hidden');
    setTimeout(() => el.remove(), 400); // match the CSS fade-out
  }

  @HostListener('document:keydown.escape')
  onEscape() {
    this.router.navigate(['/']);
  }
}
