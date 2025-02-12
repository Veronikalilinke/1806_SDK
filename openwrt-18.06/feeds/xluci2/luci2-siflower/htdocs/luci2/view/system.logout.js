L.ui.view.extend({
	execute: function() {
		L.session.destroy('ubus', 'session', 'destroy');
		location.replace('/');
	}
});
